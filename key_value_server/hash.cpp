#include <assert.h>
// The solution to resize latency problem is to do it progressively
// but resizing and rehasing need allocating new memory and initializing it, That memset+mallco is still O(n), but to solve this ??
// This is avoided with calloc(). 
// as learned calloc() resolved reallocation latency by using vistual memory and os lazy paging
// It reserves virtual memory pages (e.g., 4 KB each)
// But the actual physical memory is not allocated yet
// On first access to each page, the OS:
// Maps a physical page
// Fills it with zeros
#include <stdlib.h>     // calloc(), free()
#include "hash.h"


//initilization
// n must be a power of 2
static void h_init(Htab* htab, size_t n ){
    assert((n > 0) && (n & n - 1) == 0);
    htab->tab = (HNode**)calloc( n , sizeof(HNode*));
    htab->mask = n - 1;
    htab->size = 0;
}

//hashtable insertion
static void h_insert(Htab* htab, HNode* node){
    //instead of modulo
    //mask is for eg if n = 16, 1111
    size_t index = node->hcode & htab->mask;
    // insert at back
    HNode* next = htab->tab[index];
    node->next = next;
    //reset the end of the list to the recently added node
    htab->tab[index] = node;
    htab->size++;
    // next->next = node;
}


// basically returning the address of the pointer that points to our target
//The function returns HNode ** because it's returning the address of a pointer variable.
// hashtable look up subroutine.
// Pay attention to the return value. It returns the address of
// the parent pointer that owns the target node,
// which can be used to delete the target node.
static HNode** h_lookup(Htab* htab, HNode* key, bool (*eq)(HNode* , HNode*)){
    if(!htab->tab)return NULL;
    size_t index = htab->mask & key->hcode;
    HNode** node = &htab->tab[index];
    // HNode** parent = NULL;
    // we are returning parent of out target node.
    for(HNode* curr = *node; curr  != NULL ; curr = curr->next){
        //unique hcode but non unique index
        if(curr->hcode == key->hcode && eq(curr, key)){
            return node;
        }
        node = &curr->next;
    }
    return NULL;
}

// use delete ??? no onyl detach not delete ?
//just change the what the pointer points to, eseentially removing it 
static HNode* h_detach(Htab* htab, HNode** node){
    HNode *target = *node;
    *node = target->next;
    htab->size--;
    return target;
    //return the original previous pointer to delete it. otherwise leak(Scary!!)
}

const size_t k_rehashing_work = 128;    // constant work


// NOW COMES THE GOOD STUFF.
// REHASHING WITH PROGRESSION
// this way rehashing will be distributed among many calls.
// amortized complexity has previously still o(1) and still is now
// it is just we wont see a spike
// each time the hashtable is used, move some more keys. This can slow down 
// lookups during resizing because there are 2 hashtables to query.
static void hm_help_rehashing(HMAP *hmap){
    size_t rehashed = 0;
    while(rehashed < k_rehashing_work && hmap->prev.size > 0){
        HNode** from = &hmap->prev.tab[hmap->migrated_pos];
        // find empty slot to move its contents from the prev old to new curr
        if(!*from){
            hmap->migrated_pos++;
            continue;
        }
        //
        HNode* t = h_detach(&hmap->prev, from);
        h_insert(&hmap->curr, t);
        hmap->migrated_pos++;
        rehashed++;
        // hmap->prev.size--;
    }

    //discard the older prev table if done.
    if(hmap->prev.size == 0 && hmap->prev.tab){
        free(hmap->prev.tab);
        hmap->prev = Htab{};
    }
}

// trigger rehashing. i.e max nodes in buckets reached now resize, but also do 
// two things to reduce latency,
// use calloc to initialize the memory
// use progressing rehahsing, migrate progressively
//// (newer, older) <- (new_table, newer)
static void hm_trigger_rehashing(HMAP* hmap){
    // previous table must be null, if it is not then there are some elements left migrating
    assert(hmap->prev.tab == NULL);

    hmap->prev = hmap->curr;
    //double the size
    h_init(&hmap->curr, (hmap->curr.mask + 1)*2);
    hmap->migrated_pos = 0;

}

HNode *hm_lookup(HMAP *hmap, HNode *key, bool (*eq)(HNode *, HNode *)){
    hm_help_rehashing(hmap);
    HNode **from = h_lookup(&hmap->curr, key, eq);
    if(!from){
        from = h_lookup(&hmap->prev, key, eq);
    }
    return from ? *from : NULL;
}

// define load factor
const size_t k_max_load_factor = 8;

void  hm_insert(HMAP *hmap, HNode *node){
    // trigger rehashing if load factor exceeds.
    // load_factor = keys/slots
    // it's theoretically possible to hit a long chain, it's statistically unlikely.
    // WITH LARGE ENOUGH KEYS THIS GIVES CONSTANT TIME
    if(!hmap->curr.tab){
        h_init(&hmap->curr, 4);
    }
    h_insert(&hmap->curr, node);
    if(!hmap->prev.tab){
        size_t load = (hmap->curr.mask + 1)*k_max_load_factor;
        if(load <= hmap->curr.size){
            hm_trigger_rehashing(hmap);
        }
    }
    hm_help_rehashing(hmap);

}

// now in delete no free is done of the node, it is returned and its the 
// reponsibility of the caller to free it. its done to provide flexibility
HNode *hm_delete(HMAP *hmap, HNode *key, bool (*eq)(HNode *, HNode *)){
    hm_help_rehashing(hmap);
    // first look for the node with the key
    // then detach it

    //first look in new 
    // and then in old
    if(HNode** from = h_lookup(&hmap->curr, key, eq)){
        return h_detach(&hmap->curr, from);
    }
    if(HNode** from = h_lookup(&hmap->prev, key, eq)){
        return h_detach(&hmap->prev, from);
    }

    return NULL;
    
}
void   hm_clear(HMAP *hmap){
    free(hmap->curr.tab);
    free(hmap->prev.tab);
    *hmap = HMAP{};

}
size_t hm_size(HMAP *hmap){
    return hmap->curr.size + hmap->prev.size;
}






