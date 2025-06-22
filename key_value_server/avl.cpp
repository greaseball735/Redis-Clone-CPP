#include <assert.h>
#include "avl.h"


static uint32_t max(uint32_t lhs, uint32_t rhs) {
    return lhs < rhs ? rhs : lhs;
}

// maintain the height and cnt field
static void avl_update(AVLNode *node) {
    node->height = 1 + max(avl_height(node->left), avl_height(node->right));
    node->cnt = 1 + avl_cnt(node->left) + avl_cnt(node->right);
}

//determine left right heavy and act accordingly what rotation to apply

static AVLNode *rot_left(AVLNode *node) {
    AVLNode *parent = node->parent;
    AVLNode *new_node = node->right;
    AVLNode *inner = new_node->left;
    // node <-> inner
    node->right = inner;
    if (inner) {
        inner->parent = node;
    }
    // parent <- new_node
    new_node->parent = parent;
    // new_node <-> node
    new_node->left = node;
    node->parent = new_node;
    // auxiliary data
    avl_update(node);
    avl_update(new_node);
    return new_node;
}

static AVLNode* rot_right(AVLNode* node){
    AVLNode* parent = node->parent;
    AVLNode* left = node->left;
    AVLNode* ll = left->right;
    node->left = ll;
    left->right = node;
    //adjust parents
    /// IMPORTANT :: you are updating only the parent pointer stored in the node auxilliary data.
    // the real parent pointer, the pointer whose left or right child is now changed is still pointing to the old
    // one, has to deal with it. NEEEEED to update the child pointer inside the parent

    if(ll){
        ll->parent = node;
    }
    left->parent = parent;
    node->parent = left;
    //update heights
    avl_update(node);
    avl_update(left);
    return left;
}

static AVLNode *avl_fix_left(AVLNode *node) {
    if(avl_height(node->left->left) < avl_height(node->left->right)){
        node->left = rot_left(node->left);

    }
    //dont forget to return node values 
    return rot_right(node);
}
// the right subtree is taller by 2
static AVLNode *avl_fix_right(AVLNode *node) {
    if (avl_height(node->right->right) < avl_height(node->right->left)) {
        node->right = rot_right(node->right);
    }
    return rot_left(node);
}


// // fix imbalanced nodes and maintain invariants until the root is reached
//inserted or deleted but has to check where the invariant is lost.
// continue up the tree bottom up
//The tree height propagates from the updated node to the root, and any 
//imbalances are fixed during propagation. The fix may change the root
// node, so the root node is returned because the data structure doesn’t 
//store the root pointer.


// the **from double pointer trick i have seen in many peoples code it is a clever trick 
// so from is the pointer to the pointer that points to the subtree, it holds the actual original address of this pointer. some other pointer 
// might have this as left or right nodes, so when we do *from we are going to the actual pointer and then we replace it with some other pointer, 
// so the address of this pointer remains the same and its just it is pointing to some other address.

// /The magic is that while we're changing what the pointer points to, we're not changing where the pointer itself is stored. This ensures:
// Parent nodes automatically "see" the updated subtree
// No need to traverse back up the tree to update references
// All existing parent→child relationships maintain their storage locations



//returns root of the tree, traversing up
AVLNode *avl_fix(AVLNode *node){
    while(1){
        AVLNode** from = &node;  //assumes node is the root
        AVLNode* parent = node->parent;
        if(parent){
            from = parent->left == node ? &parent->left : &parent->right;
        }
        avl_update(node);
        uint32_t l = avl_height(node->left);
        uint32_t r = avl_height(node->right);
        if(l - r == 2){
            *from = avl_fix_left(node);
        }else if(r - l == 2){
            
            *from = avl_fix_right(node);
        }
        if(!parent){
            return *from;
        }
        
        node = parent;
    }
    return NULL;

}

// like in bst cases, first trivial case is the node has only either left or right child. then simply remove it and update the parent pointer to point to the 
// one targets's child.
// second case is both left and right subtrees, child exist, in that case find successor
// detach a node where 1 of its children is empty
static AVLNode *avl_del_easy(AVLNode *node) {
    assert(!node->left || !node->right);    // at most 1 child
    AVLNode *child = node->left ? node->left : node->right; // can be NULL
    AVLNode *parent = node->parent;
    // update the child's parent pointer
    if (child) {
        child->parent = parent; // can be NULL
    }
    // attach the child to the grandparent
    if (!parent) {
        return child;   // removing the root node
    }
    AVLNode **from = parent->left == node ? &parent->left : &parent->right;
    *from = child;
    // rebalance the updated tree
    return avl_fix(parent);
}

// detach a node and returns the new root of the tree
AVLNode *avl_del(AVLNode *node) {
    // the easy case of 0 or 1 child
    if (!node->left || !node->right) {
        return avl_del_easy(node);
    }
    // find the successor
    AVLNode *victim = node->right;
    while (victim->left) {
        victim = victim->left;
    }
    // detach the successor
    AVLNode *root = avl_del_easy(victim);
    // swap with the successor
    *victim = *node;    // left, right, parent
    if (victim->left) {
        victim->left->parent = victim;
    }
    if (victim->right) {
        victim->right->parent = victim;
    }
    // attach the successor to the parent, or update the root pointer
    AVLNode **from = &root;
    AVLNode *parent = node->parent;
    if (parent) {
        from = parent->left == node ? &parent->left : &parent->right;
    }
    *from = victim;
    return root;
}



AVLNode *avl_offset(AVLNode *node, int64_t offset){

    
}