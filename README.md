# Redis-Like In-Memory Data Store in C++

This project is a minimal, high-performance, Redis-like in-memory key-value data store built from scratch in C++. It follows the design philosophy of Redis, with a focus on speed, extensibility, and efficient data structures.

## Features

- Custom hashtable implementation. (STL containers avoided for latency and control)
- Intrusive data structures for efficient memory usage, high cache performance and fast pointer-based lookup
- Supports both simple string keys and advanced sorted sets
- Clean command interface for interacting via CLI.
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

## Internal Design

### Data Model

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

## Why This Design

- Minimal latency and control over memory layout
- Understand internals of systems like Redis at a deep level
- Avoid STL overhead for tight control over allocations, hashing, and pointer layout
- Serve as a learning tool for intrusive design, tagged unions, and manual memory handling

## How to Build

```bash
g++ -std=c++17 -O2 -Wall -o myredis main.cpp
