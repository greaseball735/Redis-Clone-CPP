#!/usr/bin/env python3
"""
Comprehensive performance testing suite for Redis-like TCP server
Tests: Concurrency, Throughput, Latency, Load Testing
"""

import socket
import struct
import time
import threading
import asyncio
import statistics
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import sys
from typing import List, Tuple, Dict, Any
import random
import string

class RedisClient:
    def __init__(self, host='127.0.0.1', port=1234):
        self.host = host
        self.port = port
        self.socket = None
    
    def connect(self):
        """Connect to the Redis server"""
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((self.host, self.port))
    
    def disconnect(self):
        """Disconnect from the Redis server"""
        if self.socket:
            self.socket.close()
            self.socket = None
    
    def send_command(self, cmd_args: List[str]) -> bytes:
        """Send command to server and return response"""
        # Calculate message length
        msg_len = 4  # for number of arguments
        for arg in cmd_args:
            msg_len += 4 + len(arg.encode())
        
        # Build message
        buf = bytearray()
        buf.extend(struct.pack('<I', msg_len))  # Total length
        buf.extend(struct.pack('<I', len(cmd_args)))  # Number of args
        
        for arg in cmd_args:
            arg_bytes = arg.encode()
            buf.extend(struct.pack('<I', len(arg_bytes)))
            buf.extend(arg_bytes)
        
        # Send message
        self.socket.sendall(buf)
        
        # Read response
        response_len_bytes = self._read_full(4)
        response_len = struct.unpack('<I', response_len_bytes)[0]
        response_data = self._read_full(response_len)
        
        return response_data
    
    def _read_full(self, n: int) -> bytes:
        """Read exactly n bytes from socket"""
        buf = b''
        while len(buf) < n:
            chunk = self.socket.recv(n - len(buf))
            if not chunk:
                raise ConnectionError("Unexpected EOF")
            buf += chunk
        return buf

class PerformanceTester:
    def __init__(self, host='127.0.0.1', port=1234):
        self.host = host
        self.port = port
        self.results = {}
    
    def generate_random_key(self, length=10) -> str:
        """Generate random key for testing"""
        return ''.join(random.choices(string.ascii_letters + string.digits, k=length))
    
    def generate_random_value(self, length=100) -> str:
        """Generate random value for testing"""
        return ''.join(random.choices(string.ascii_letters + string.digits + ' ', k=length))
    
    def test_basic_connectivity(self) -> bool:
        """Test basic server connectivity"""
        print("Testing basic connectivity...")
        try:
            client = RedisClient(self.host, self.port)
            client.connect()
            
            # Test simple SET/GET operations
            key = self.generate_random_key()
            value = self.generate_random_value()
            
            client.send_command(['set', key, value])
            response = client.send_command(['get', key])
            
            client.disconnect()
            print("✓ Basic connectivity test passed")
            return True
        except Exception as e:
            print(f"✗ Basic connectivity test failed: {e}")
            return False
    
    def test_single_thread_throughput(self, duration=10, operation='set') -> Dict[str, Any]:
        """Test single-thread throughput"""
        print(f"Testing single-thread throughput ({operation}) for {duration}s...")
        
        client = RedisClient(self.host, self.port)
        client.connect()
        
        start_time = time.time()
        operations = 0
        latencies = []
        
        try:
            while time.time() - start_time < duration:
                key = self.generate_random_key()
                value = self.generate_random_value()
                
                op_start = time.time()
                if operation == 'set':
                    client.send_command(['set', key, value])
                elif operation == 'get':
                    client.send_command(['get', key])
                elif operation == 'del':
                    client.send_command(['del', key])
                op_end = time.time()
                
                latencies.append((op_end - op_start) * 1000)  # Convert to ms
                operations += 1
        
        finally:
            client.disconnect()
        
        actual_duration = time.time() - start_time
        throughput = operations / actual_duration
        
        result = {
            'operations': operations,
            'duration': actual_duration,
            'throughput_ops_sec': throughput,
            'avg_latency_ms': statistics.mean(latencies) if latencies else 0,
            'min_latency_ms': min(latencies) if latencies else 0,
            'max_latency_ms': max(latencies) if latencies else 0,
            'p50_latency_ms': statistics.median(latencies) if latencies else 0,
            'p95_latency_ms': statistics.quantiles(latencies, n=20)[18] if len(latencies) > 20 else 0,
            'p99_latency_ms': statistics.quantiles(latencies, n=100)[98] if len(latencies) > 100 else 0
        }
        
        print(f"✓ Single-thread {operation}: {throughput:.1f} ops/sec, avg latency: {result['avg_latency_ms']:.2f}ms")
        return result
    
    def worker_thread(self, thread_id: int, duration: int, operation: str, barrier: threading.Barrier) -> Dict[str, Any]:
        """Worker thread for concurrent testing"""
        client = RedisClient(self.host, self.port)
        client.connect()
        
        # Wait for all threads to be ready
        barrier.wait()
        
        start_time = time.time()
        operations = 0
        latencies = []
        errors = 0
        
        try:
            while time.time() - start_time < duration:
                key = f"thread_{thread_id}_{self.generate_random_key()}"
                value = self.generate_random_value()
                
                try:
                    op_start = time.time()
                    if operation == 'set':
                        client.send_command(['set', key, value])
                    elif operation == 'get':
                        client.send_command(['get', key])
                    elif operation == 'mixed':
                        # Mix of operations
                        if random.random() < 0.7:  # 70% reads
                            client.send_command(['get', key])
                        else:  # 30% writes
                            client.send_command(['set', key, value])
                    op_end = time.time()
                    
                    latencies.append((op_end - op_start) * 1000)
                    operations += 1
                except Exception as e:
                    errors += 1
        
        finally:
            client.disconnect()
        
        return {
            'thread_id': thread_id,
            'operations': operations,
            'errors': errors,
            'latencies': latencies
        }
    
    def test_concurrent_throughput(self, num_threads=10, duration=10, operation='set') -> Dict[str, Any]:
        """Test concurrent throughput with multiple threads"""
        print(f"Testing concurrent throughput ({operation}) with {num_threads} threads for {duration}s...")
        
        barrier = threading.Barrier(num_threads)
        
        with ThreadPoolExecutor(max_workers=num_threads) as executor:
            futures = [
                executor.submit(self.worker_thread, i, duration, operation, barrier)
                for i in range(num_threads)
            ]
            
            results = [future.result() for future in as_completed(futures)]
        
        # Aggregate results
        total_operations = sum(r['operations'] for r in results)
        total_errors = sum(r['errors'] for r in results)
        all_latencies = []
        for r in results:
            all_latencies.extend(r['latencies'])
        
        throughput = total_operations / duration
        
        result = {
            'num_threads': num_threads,
            'total_operations': total_operations,
            'total_errors': total_errors,
            'duration': duration,
            'throughput_ops_sec': throughput,
            'avg_latency_ms': statistics.mean(all_latencies) if all_latencies else 0,
            'min_latency_ms': min(all_latencies) if all_latencies else 0,
            'max_latency_ms': max(all_latencies) if all_latencies else 0,
            'p50_latency_ms': statistics.median(all_latencies) if all_latencies else 0,
            'p95_latency_ms': statistics.quantiles(all_latencies, n=20)[18] if len(all_latencies) > 20 else 0,
            'p99_latency_ms': statistics.quantiles(all_latencies, n=100)[98] if len(all_latencies) > 100 else 0,
            'error_rate': total_errors / (total_operations + total_errors) if (total_operations + total_errors) > 0 else 0
        }
        
        print(f"✓ Concurrent {operation}: {throughput:.1f} ops/sec, avg latency: {result['avg_latency_ms']:.2f}ms, errors: {total_errors}")
        return result
    
    def test_latency_under_load(self, concurrent_load=5, test_duration=10) -> Dict[str, Any]:
        """Test latency under concurrent load"""
        print(f"Testing latency under load ({concurrent_load} background threads)...")
        
        # Start background load
        load_barrier = threading.Barrier(concurrent_load + 1)  # +1 for main test thread
        stop_event = threading.Event()
        
        def background_load():
            client = RedisClient(self.host, self.port)
            client.connect()
            load_barrier.wait()
            
            while not stop_event.is_set():
                try:
                    key = self.generate_random_key()
                    value = self.generate_random_value()
                    client.send_command(['set', key, value])
                    time.sleep(0.001)  # Small delay to avoid overwhelming
                except:
                    pass
            client.disconnect()
        
        # Start background threads
        load_threads = []
        for _ in range(concurrent_load):
            t = threading.Thread(target=background_load)
            t.start()
            load_threads.append(t)
        
        # Wait for all background threads to be ready
        load_barrier.wait()
        
        # Measure latency of individual operations
        client = RedisClient(self.host, self.port)
        client.connect()
        
        latencies = []
        start_time = time.time()
        
        try:
            while time.time() - start_time < test_duration:
                key = self.generate_random_key()
                value = self.generate_random_value()
                
                op_start = time.time()
                client.send_command(['get', key])  # Test GET latency
                op_end = time.time()
                
                latencies.append((op_end - op_start) * 1000)
                time.sleep(0.01)  # Small delay between measurements
        
        finally:
            client.disconnect()
            stop_event.set()
            
            # Wait for background threads to finish
            for t in load_threads:
                t.join()
        
        result = {
            'concurrent_load_threads': concurrent_load,
            'test_operations': len(latencies),
            'avg_latency_ms': statistics.mean(latencies) if latencies else 0,
            'min_latency_ms': min(latencies) if latencies else 0,
            'max_latency_ms': max(latencies) if latencies else 0,
            'p50_latency_ms': statistics.median(latencies) if latencies else 0,
            'p95_latency_ms': statistics.quantiles(latencies, n=20)[18] if len(latencies) > 20 else 0,
            'p99_latency_ms': statistics.quantiles(latencies, n=100)[98] if len(latencies) > 100 else 0
        }
        
        print(f"✓ Latency under load: avg {result['avg_latency_ms']:.2f}ms, p95 {result['p95_latency_ms']:.2f}ms")
        return result
    
    def test_connection_handling(self, max_connections=100000) -> Dict[str, Any]:
        """Test server's ability to handle multiple connections"""
        print(f"Testing connection handling (up to {max_connections} connections)...")
        
        successful_connections = 0
        failed_connections = 0
        clients = []
        
        try:
            for i in range(max_connections):
                try:
                    client = RedisClient(self.host, self.port)
                    client.connect()
                    clients.append(client)
                    successful_connections += 1
                except Exception as e:
                    failed_connections += 1
                    if i < 10:  # Only print first few errors
                        print(f"Connection {i} failed: {e}")
                    break
        
        finally:
            # Clean up connections
            for client in clients:
                try:
                    client.disconnect()
                except:
                    pass
        
        result = {
            'max_attempted': max_connections,
            'successful_connections': successful_connections,
            'failed_connections': failed_connections,
            'connection_success_rate': successful_connections / max_connections
        }
        
        print(f"✓ Connection handling: {successful_connections}/{max_connections} successful")
        return result
    
    def run_full_test_suite(self) -> Dict[str, Any]:
        """Run complete performance test suite"""
        print("=" * 60)
        print("Redis Server Performance Test Suite")
        print("=" * 60)
        
        if not self.test_basic_connectivity():
            print("Basic connectivity failed. Aborting tests.")
            return {}
        
        results = {}
        
        # Single-thread tests
        results['single_thread_set'] = self.test_single_thread_throughput(duration=5, operation='set')
        results['single_thread_get'] = self.test_single_thread_throughput(duration=5, operation='get')
        
        # Concurrent tests with different thread counts
        for threads in [5, 10, 20, 50]:
            results[f'concurrent_set_{threads}t'] = self.test_concurrent_throughput(
                num_threads=threads, duration=10, operation='set'
            )
            results[f'concurrent_get_{threads}t'] = self.test_concurrent_throughput(
                num_threads=threads, duration=10, operation='get'
            )
            results[f'concurrent_mixed_{threads}t'] = self.test_concurrent_throughput(
                num_threads=threads, duration=10, operation='mixed'
            )
        
        # Latency under load
        results['latency_under_load'] = self.test_latency_under_load()
        
        # Connection handling
        results['connection_handling'] = self.test_connection_handling()
        
        # Print summary
        self.print_summary(results)
        
        return results
    
    def print_summary(self, results: Dict[str, Any]):
        """Print test results summary"""
        print("\n" + "=" * 60)
        print("PERFORMANCE TEST SUMMARY")
        print("=" * 60)
        
        # Single-thread performance
        if 'single_thread_set' in results:
            print(f"Single-thread SET: {results['single_thread_set']['throughput_ops_sec']:.1f} ops/sec")
        if 'single_thread_get' in results:
            print(f"Single-thread GET: {results['single_thread_get']['throughput_ops_sec']:.1f} ops/sec")
        
        # Best concurrent performance
        best_concurrent = 0
        best_threads = 0
        for key, result in results.items():
            if 'concurrent' in key and 'throughput_ops_sec' in result:
                if result['throughput_ops_sec'] > best_concurrent:
                    best_concurrent = result['throughput_ops_sec']
                    best_threads = result['num_threads']
        
        if best_concurrent > 0:
            print(f"Best concurrent performance: {best_concurrent:.1f} ops/sec ({best_threads} threads)")
        
        # Latency summary
        if 'latency_under_load' in results:
            lat_result = results['latency_under_load']
            print(f"Latency under load - Avg: {lat_result['avg_latency_ms']:.2f}ms, "
                  f"P95: {lat_result['p95_latency_ms']:.2f}ms, P99: {lat_result['p99_latency_ms']:.2f}ms")
        
        # Connection handling
        if 'connection_handling' in results:
            conn_result = results['connection_handling']
            print(f"Max concurrent connections: {conn_result['successful_connections']}")
        
        print("=" * 60)

def main():
    parser = argparse.ArgumentParser(description='Redis Server Performance Tester')
    parser.add_argument('--host', default='127.0.0.1', help='Server host')
    parser.add_argument('--port', type=int, default=1234, help='Server port')
    parser.add_argument('--output', help='Output results to JSON file')
    parser.add_argument('--quick', action='store_true', help='Run quick tests only')
    
    args = parser.parse_args()
    
    tester = PerformanceTester(args.host, args.port)
    
    if args.quick:
        print("Running quick performance tests...")
        results = {}
        results['connectivity'] = tester.test_basic_connectivity()
        results['single_thread'] = tester.test_single_thread_throughput(duration=3)
        results['concurrent_10t'] = tester.test_concurrent_throughput(num_threads=10, duration=5)
    else:
        results = tester.run_full_test_suite()
    
    if args.output and results:
        with open(args.output, 'w') as f:
            json.dump(results, f, indent=2)
        print(f"Results saved to {args.output}")

if __name__ == '__main__':
    main()