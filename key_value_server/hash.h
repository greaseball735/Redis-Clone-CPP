#pragma once

#include <stddef.h>
#include <stdint.h>

// in normal intrusive hash table implementaion like STL unorderd_map
// The hash table manages its own internal node structure.
// You pass the key and value to it.
// It dynamically allocates nodes internally (often with key, value, hash, and next pointer).
// This is generic but has problems like -----------------------------------------------
// Extra memory allocations (each insert = heap allocation).
// Cannot control layout of memory (not cache-friendly).
// No way to reduce latency spikes or optimize for low-level performance.

//data contains the stucture. a new design
// usually strucutre contains data.like the standard linked list implementaiton with node struct with nex and data vars.


// DATA CONTAINS THE STRUCTURE


// The hash table is only responsible for managing linked chains of HNode pointers. It doesn’t allocate or manage your MyData.

// struct HNode {
//     HNode *next = nullptr;
//     uint64_t hcode = 0;
// };

// struct MyData {
//     std::string key;
//     std::string value;
//     HNode node; // embedded
// };
// Now your hash table doesn't store MyData directly — it stores HNode*, and you manually access your struct via:


// THIS HAS BENIFITS LIKE 
//You allocate MyData, hash table uses it directly (no wrappers).
// No dynamic memory per node = better cache performance, less GC/latency.
// Can use the same data structure across multiple lists/tables.
// Fine-grained control over memory layout, lifetime, and behavior.

// Non-intrusive = You give someone your data and they wrap it however they want.
// Intrusive = You prepare your data in their format so they can use it directly without copying or wrapping.


// hashtable node, should be embedded into the payload
// each locaton has next pointer to support collision handling using chaining(linked lists)
struct HNode {
    HNode *next = NULL;
    //pre computed hash of the key 
    uint64_t hcode = 0;
};

// a simple fixed-sized hashtable
// the low level, has no resizing, has no key value semantics, just manages
// buckets of HNode* linked lists
struct Htab {
    HNode **tab = NULL; // array of slots
    // not sure about this var
    // TO AVOID expensive % operation 
    // index = hcode & mask
    size_t mask = 0;    // power of 2 array size, 2^n - 1
    size_t size = 0;    // number of keys
};

// the real hashtable interface.
// it uses 2 hashtables for progressive rehashing.
// the real stuff, high level wrapper. implements features
struct HMAP {
    Htab curr;
    Htab prev;
    size_t migrated_pos = 0;
};

// passed key as HNode* this is a generic, intrusive hash table, HNode is not the key itself — it's part of the payload.
// example usage
// struct MyData {
    // std::string key;
    // std::string value;
    // HNode hnode; // embedded HNode
// };


//PASSING FUNCTION AS PARAMETER.. COOL ! function is just some bytes of instructions sitting in memory. can have pointers to that 
//gives flexibilty to use different comparision fucntions for lookup
HNode *hm_lookup(HMAP *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void   hm_insert(HMAP *hmap, HNode *node);
HNode *hm_delete(HMAP *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void   hm_clear(HMAP *hmap);
size_t hm_size(HMAP *hmap);

