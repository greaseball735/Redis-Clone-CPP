#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
// proj
#include "range.h"
#include "helper.h"
#include "avl.h"
//goal
// A set of name/score pairs
// Efficient support for ranked/range access by score → AVL Tree
// Efficient lookup by name → Hash Table

//THE DESIGN
//earlier, one entry, one global hashmap. key maps to value. simple.

//NOW we can have things like
// key1 - {(name1, score1) , (name2, score2), ..... }
// a key associated with a set. this set is ordered according to score and then name value,score is a double
// how to design this ??

// so basically entry can now have 2 fields a string or a zset as value.
// ok so add these fields in Entry, only one of them is active at a time.

// Now how to implement this set ?
// the data structure used is a avl tree. ok fine then what ? how to orgnaize it.
// ok so each entry is associated with a key.
// a key can be associated with a set.
// a set is represented as a tree AND  a hashmap to query by name, as name is unique.
// so define the ZSet, embed, a hashmap and a root of the set in it.

// then what ?? we are using intrusive data strucutre. so embed the AVLNode in the data
// the data is stored in ZNode. with score, name, len and avlnode information.

// the hash map here maps keys to their corresponding ZNode
// struct ZSet{
//     AVLNode *root = NULL;
//     //name to score map
//     HMAP hmap;
// };
//intrusive
// data contains the links or nodes (like pointers for tree or list).
// The data is aware of its participation in the structure.
// No separate allocation of node objects — only the data is allocated, and it embeds the linking structure.

// these nodes are managed by Zset
// struct ZNode {
//     //ZNode contains AVLNode tree and HNode hmap directly inside itsel
//     // There is one object (ZNode) which is inserted into two data structures: an AVL tree and a hash map.
//     // These structures don’t store ZNode* as generic values — instead, they operate on tree and hmap, and later convert back to ZNode*.
//     // You don’t allocate AVLNode or HNode separately.
// // You get better cache locality, fewer allocations, and less memory overhead
    

//     //data strucutre nodes
//     AVLNode tree;
//     HNode hmap;

//     //data
//     double score;
//     size_t len;
//     char name[0];
//     // flexible array memeber, we can extend and allocate more memory at the end of the struct 



// };
// helper struct
// i dunno this seems unnecssary. but has to store all this info somewhere.
// hnode only contains next and hashkey
struct HKey {
    HNode node;
    const char *name = NULL;
    size_t len = 0;
};


//create new znode
static ZNode* znode_new(double score, size_t len, const char* name){
    ZNode* node = (ZNode*)malloc(sizeof(ZNode) + len);
    avl_init(&node->tree);
    node->hmap.next = NULL;
    node->hmap.hcode = str_hash((uint8_t*)name, len);
    node->score = score;
    node->len = len;
    memcpy(&node->name[0], name, len);
    return node;
}
static void znode_del(ZNode *node) {
    free(node);
}

static bool hcmp(HNode *node, HNode *key) {
    ZNode *znode = container_of(node, ZNode, hmap);
    HKey *hkey = container_of(key, HKey, node);
    if (znode->len != hkey->len) {
        return false;
    }
    //memcmp. compare raw bytes. 
    //strcmp works only with Works only with null-terminated C-strings (char* ending in '\0')
    return 0 == memcmp(znode->name, hkey->name, znode->len);
}

//returns 1 if left < right
//else 0
//fuck this shit
//need to make another fucking comparision. comparision with fucking score 
// ??? compariosn k liye bc ek naya struck tmc ]asdfk;asdf
static uint8_t z_comp_2(AVLNode* target , double score, size_t len, const char* name){
    ZNode *z  = container_of(target , ZNode , tree);
    if(z->score != score){
        return z->score < score;
    }
    int rv = memcmp(z->name, name, std::min(z->len, len));
    if (rv != 0) {
        return rv < 0;
    }
    if(z->len == len){
        return 2;
    }else{
        return z->len < len;

    }
}

static bool z_comp(AVLNode* l, AVLNode* r){
    //made my life difficult by going intrusive.
    ZNode* z1 = container_of(l, ZNode, tree);
    ZNode* z2 = container_of(r, ZNode, tree);
    if(z1->score != z2->score){
        return z1->score < z2->score;
    }
    // order by score then order by name
    int rv = memcmp(z1->name, z2->name, std::min(z1->len, z2->len));
    if(rv == 0){
        return (z1->len < z2->len);
    }else{
        return rv < 0;
    }


}
static void tree_insert(ZSet* zset, ZNode* node){
    AVLNode* parent = NULL;
    AVLNode** from = &zset->root;
    while(*from){
        parent = *from;
        from = z_comp(&node->tree, parent) ? &parent->left : &parent->right;
        //has to compare score
    }
    //node to be inserted
    *from = &node->tree;
    node->tree.parent = parent;
    zset->root = avl_fix(&node->tree);

}


ZNode *zset_lookup(ZSet *zset, const char *name, size_t len){
    //simple key value lookup   
    // if (!zset->tree) {
    //     return NULL;
    // }
    HKey key;
    key.node.hcode = str_hash((uint8_t *)name, len);
    key.name = name;
    key.len = len;
    HNode *found = hm_lookup(&zset->hmap, &key.node, &hcmp);
    return found ? container_of(found, ZNode, hmap) : NULL;

}
// Detaching and re-inserting the AVL node will fix the order if the score changes.
static void zset_update(ZSet *zset, ZNode* node, double score){
     // detach the tree node
    zset->root = avl_del(&node->tree);
    avl_init(&node->tree);
    // reinsert the tree node
    node->score = score;
    tree_insert(zset, node);
}
bool zset_insert(ZSet *zset, const char *name, size_t len, double score){
    if(ZNode* node = zset_lookup(zset, name, len)){
        zset_update(zset, node , score);
        return false;
    }
    ZNode* node = znode_new(score, len ,name);
    hm_insert(&zset->hmap, &node->hmap);
    tree_insert(zset, node);
    return true;

    //check if tuple pair, already exist
    // if not then first insert into hash map 
    // then insert into avl tree
}
void zset_delete(ZSet *zset, ZNode *node){
    // remove from the hashtable
    HKey key;
    key.node.hcode = node->hmap.hcode;
    key.name = node->name;
    key.len = node->len;
    HNode *found = hm_delete(&zset->hmap, &key.node, &hcmp);
    assert(found);
    // remove from the tree
    zset->root = avl_del(&node->tree);
    // deallocate the node
    znode_del(node);


}


//RANGE QUERY
//to handle this query
// The range query command: ZQUERY key score name offset limit.
// Seek to the first pair where pair >= (score, name).
// Walk to the n-th successor/predecessor (offset).
// Iterate and output.

ZNode* zset_ceil(ZSet* zset, double score, const char *name, size_t len){
    AVLNode *found = NULL;
    AVLNode *node = zset->root;
    while(node){
        uint8_t c = z_comp_2(node, score, len, name);
        if(c == 1){
            node = node->right; // node < key
        }else if(c == 0){
            found = node;
            node = node->left;
        }else{
            found = node;
            break;
        }

    }
    return found ? container_of(found, ZNode, tree) : NULL;
}

int64_t zset_rank(ZSet* zset, double score, const char *name, size_t len){
    int64_t rank = 0;
    AVLNode *node = zset->root;
    while(node){
        
        uint8_t c = z_comp_2(node, score, len, name);
        if(c == 0){
            // rank = node->cnt;
            node = node->left;
            // return 
        }else if(c == 1){
            int left =  avl_cnt(node->left);
            rank = 1 + left + rank;
            node = node->right;
        }else{
            rank = rank + avl_cnt(node->left);
            break;
        }
    }
    
    return rank;
}

ZNode *znode_offset(ZNode *node, int64_t offset) {
    AVLNode *tnode = node ? avl_offset(&node->tree, offset) : NULL;
    return tnode ? container_of(tnode, ZNode, tree) : NULL;
}

//free every node
static void tree_dispose(AVLNode *node) {
    if (!node) {
        return;
    }
    tree_dispose(node->left);
    tree_dispose(node->right);
    znode_del(container_of(node, ZNode, tree));
}

// destroy the zset
void zset_clear(ZSet *zset) {
    hm_clear(&zset->hmap);
    tree_dispose(zset->root);
    zset->root = NULL;
}