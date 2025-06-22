#pragma once

#include "avl.h"
#include "hash.h"


struct ZSet {
    AVLNode *root = NULL;   // index by (score, name)
    HMAP hmap;              // index by name
};

struct ZNode {
    AVLNode tree;
    HNode   hmap;
    double  score = 0;
    size_t  len = 0;
    char    name[0];        // flexible array
};

bool   zset_insert(ZSet *zset, const char *name, size_t len, double score);
ZNode *zset_lookup(ZSet *zset, const char *name, size_t len);
void   zset_delete(ZSet *zset, ZNode *node);
ZNode *zset_search(ZSet *zset, double score, const char *name, size_t len);
void   zset_clear(ZSet *zset);
ZNode *znode_offset(ZNode *node, int64_t offset);


