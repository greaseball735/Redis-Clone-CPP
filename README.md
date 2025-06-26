# Redis-Like In-Memory Data Store in C++

This project is a minimal, high-performance, Redis-like in-memory key-value data store built from scratch in C++. It follows the design philosophy of Redis, with a focus on speed, extensibility, and efficient data structures.

## Features

- No use of STL containers like map, unordered_map, set.(vectors are used since I tried using a ring buffer but that made it worse)
- Increased Throughput using pipelining and non-blocking I/O.
- Uses a basic event loop to support concurrency. Supports upto 30k concurrent connections using poll() based event loop with consistent latency.
- Custom hashtable implementation for better latency and performance(30-60% performance increase).
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
  - ============================================================
  - Single-thread SET: 11086.7 ops/sec
  - Single-thread GET: 12238.5 ops/sec
  - Single-thread ZADD/ZQUERY: ~15k ops/sec 
  - Best concurrent performance: 12859.7 ops/sec (5 threads)
  - Latency under load - Avg: 0.20ms, P95: 0.53ms, P99: 1.22ms
  - Max concurrent connections: 28231
    
- These numbers are nothing compared to the real thing which can handle 100-150k ops/sec and <1 ms latency, but I am here to learn not beat a system optimized for a decade.
- They can be reproduced with the testing scripts in the ./Testing dir of the project
- But nevertheless i am happy with these results, and the most fun part for me is to try and improve these numbers
- HashTable implementation beats both std::map and std::unordered_map on both insert and lookup times.
- ![Figure_11](https://github.com/user-attachments/assets/53f813e5-9b30-4a8c-86c0-229388fde5b2)
  - HashTable is 31.3% faster than std::unordered_map for lookups
  - HashTable is 69.6% faster than std::unordered_map for insertions
- The avl tree implementation again follows intrusive design, with optimization that gives rank of an entry is O(logn) time, in practice this performs slightly worse than the std::set.

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

## A WebApp using RAM-DB as DataBase.
- I also made a small web-app that uses the RAM-DB database to simulate a leaderboard.
- A leaderboard is a very real place where a redis like database can be used to make operations faster.
  ```bash
  npm install
  cd app/
  #start the bridge, this basically acts like a serializer for the main backend DB which can only communicate in binary.
  node bridge.js
  ```
- ![Screenshot From 2025-06-26 17-52-18](https://github.com/user-attachments/assets/c15a6c9c-d59a-4df4-82d8-f7cffea04b2a)

  
- ![Screenshot From 2025-06-26 17-53-05](https://github.com/user-attachments/assets/7a7dc87d-5218-4e03-9dbd-fced9ecc01ba)

  
- ![Screenshot From 2025-06-26 17-53-15](https://github.com/user-attachments/assets/c3dcf0c4-4276-430f-b485-41b55ad754a5)
## How to Build
- the build process is disorganized now.
- the client file is client_3 in CLI directory.
- first run the server using cmake or alternatively.
```bash
cd key_value_server
g++ server_4.cpp hash.cpp helper.h avl.cpp range.cpp -o server_4
./server_4
```
- cmake
```bash
//in the project root
mkdir -p build
cd build
cmake ..
make
./server_4
```
- after running the server on localhost run the client_3.
- ![Screenshot 2025-06-26 182144](https://github.com/user-attachments/assets/7785f04c-5b2b-4872-8662-ee519e37de5a)
