# Redis-Like In-Memory Data Store in C++

This project is a minimal, high-performance, Redis-like in-memory key-value data store built from scratch in C++. It follows the design philosophy of Redis, with a focus on speed, extensibility, and efficient data structures.

## Features

- No use of STL containers like map, unordered_map, set.(vectors are used since I tried using a ring buffer but that made it worse)
- Increased Throughput using pipelining and non-blocking I/O.
- Supports upto 30k concurrent connections using poll() based syscall event with consistent latency.
- Custom hashtable implementation for better latency and performance(30-60% performance increase)
- Use of Intrusive data structures for efficient memory usage, high cache performance and fast pointer-based lookup
- Supports both simple string keys using HashMap and advanced sorted sets using AVL trees data structure.
- Clean command interface for interacting via CLI.
- Use of a simple binary Tag-Value-Length serializer protocol for communications, can support multiple data types like arrays, strings, floats.
- Manual memory management with malloc/free where required

## Supported Commands

### String Commands

- `SET key value`  
  Sets the string value of a key.

- `GET key`  
  Gets the string value of a key.

- `DEL key`  
  Deletes a key from the store.

- `KEYS`  
  Lists all keys currently in the store.

### Sorted Set Commands

- `ZADD key score name`  
  Adds an element with a given score to the sorted set `key`. If the element exists, its score is updated.

- `ZREM key name`  
  Removes the element from the sorted set.

- `ZSCORE key name`  
  Returns the score of the element.

- `ZRANK key name`  
  Returns the 0-based rank of the element in the sorted set.

- `ZQUERY key score name offset limit`  
  Range query in the sorted set:
  - Starts at the first element ≥ `(score, name)`
  - Applies `offset` and returns up to `limit` elements in sorted order

## Performance
- I tested using a python script with the server running on localhost. I am sure there are better ways and metrics to test.
- PERFORMANCE TEST SUMMARY
  - Single-thread SET: 11119.3 ops/sec
  - Single-thread GET: 10634.7 ops/sec
  - Best concurrent performance: 9733.5 ops/sec (5 threads)
  - Latency under load - Avg: 0.99ms, P95: 1.45ms, P99: 2.34ms
  - Max concurrent connections: 28231
- These numbers are nothing compared to the real thing which can handle 100-150k ops/sec and <1 ms latency, but I am here to learn not beat a system optimized for a decade.
- But nevertheless i am happy with these results, and the most fun part for me is to try and improve these numbers


## Design and Data Objects

- `Entry` objects are stored in a top-level hashtable (`store.db`)
- Each `Entry` can either store:
  - A string value
  - A sorted set (`ZSet`)

### Sorted Set

- Internally composed of:
  - An intrusive AVL tree for score-based sorted ordering
  - A hashtable for fast name-based lookup
- `ZNode` objects are doubly-indexed via both structures
- Flexible array members used to embed strings directly in nodes (to avoid extra allocations)

### Intrusive Data Structures

- All data structures use intrusive design: nodes store linkage pointers (like `HNode`, `AVLNode`) directly inside the value structs
- Only one memory allocation per logical object
- Manual constructor/destructor logic for unions involving `std::string` and custom types

### AVL Tree

- Custom AVL tree implementation supporting:
  - Balanced insert/delete
  - Successor/predecessor traversal
  - Custom comparison on `(score, name)`

## How to Build
- the build process is disorganized now.
- the client CLI file is client_3 in key_value_server directory.
- first run the server using cmake or alternatively.
```bash
cd key_value_server
g++ server_4.cpp hash.cpp helper.h avl.cpp range.cpp -o server_4
./server_4
```
- after running the server on localhost run the client_3.
- cmake
```bash
//in the project root
mkdir -p build
cd build
cmake ..
make
./server_4

