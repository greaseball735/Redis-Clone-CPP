#include <iostream>
#include <map>
#include <unordered_map>
#include <chrono>
#include <random>
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
// Replace this with your hash table class
#include "./key_value_server/hash.h"

using namespace std;
using namespace std::chrono;

const int NUM_ELEMENTS = 1'000'000;
struct Entry{
    int key;
    int val;
    HNode hnode;
};
static uint64_t int_hash(uint64_t x) {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;
        return x;
}

bool eq(HNode* f, HNode* s){
    Entry* ff  = container_of(f, Entry, hnode);
    Entry* ss  = container_of(s, Entry, hnode);
    return ff->key == ss->key;
}
int main() {
    mt19937 rng(42);
    mt19937 rng_lookup(51);
    uniform_int_distribution<int> dist(1, 1e9);
    
    vector<int> keys(NUM_ELEMENTS);
    for (int &k : keys) k = dist(rng);
    
    vector<int> keys2(NUM_ELEMENTS);
    uniform_int_distribution<int> dist_lookup(1, 1e9);
    for (int &k : keys2) k = dist(rng_lookup);

    // ==== Benchmark STL Map ====
    map<int, int> stl_map;
    auto start = high_resolution_clock::now();
    for (int k : keys) stl_map[k] = k;
    auto end = high_resolution_clock::now();
    cout << "STL map insert: " << duration_cast<milliseconds>(end - start).count() << " ms\n";
    start = high_resolution_clock::now();
    for (int k : keys2){
        auto it = stl_map.find(k);  
    } 
    end = high_resolution_clock::now();
    cout << "STL map lookup: " << duration_cast<milliseconds>(end - start).count() << " ms\n";
    
    // ==== Benchmark Unordered Map ====
    unordered_map<int, int> stl_umap;
    start = high_resolution_clock::now();
    for (int k : keys) stl_umap[k] = k;
    end = high_resolution_clock::now();
    cout << "STL unordered_map insert: " << duration_cast<milliseconds>(end - start).count() << " ms\n";
    start = high_resolution_clock::now();
    for (int k : keys2){
        auto it = stl_umap.find(k);  
    } 
    end = high_resolution_clock::now();
    cout << "STL unordered_map lookup: " << duration_cast<milliseconds>(end - start).count() << " ms\n";
    
    // ==== Benchmark Your Hash Table ====
    static struct{
        HMAP hmap;
    }store;
    start = high_resolution_clock::now();
    for (int k : keys) {
        Entry* e = new Entry();
        e->key = k;
        e->val = k;
        e->hnode.hcode = int_hash(static_cast<uint64_t>(k));
        hm_insert(&store.hmap, &e->hnode);
    }
    end = high_resolution_clock::now();
    cout << "MyHashTable insert: " << duration_cast<milliseconds>(end - start).count() << " ms\n";
    start = high_resolution_clock::now();
    for (int k : keys2){
        HNode* hn = new HNode();
        hn->hcode = int_hash(static_cast<uint64_t>(k));
        HNode* r = hm_lookup(&store.hmap, hn, eq);  
    } 
    end = high_resolution_clock::now();
    cout << "MyHashTable lookup: " << duration_cast<milliseconds>(end - start).count() << " ms\n";
    
    return 0;
}
// myhashtable lookup time is very high. is there a problem with how i am testing it ? 