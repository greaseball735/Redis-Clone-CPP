#include <iostream>
#include <set>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>
#include <cassert>
#include <cstddef>  // For offsetof

// First, let's create the header definitions your AVL tree needs
struct AVLNode {
    AVLNode *left = nullptr;
    AVLNode *right = nullptr;
    AVLNode *parent = nullptr;
    uint32_t height = 1;
    uint32_t cnt = 1;
};

// Helper functions that your AVL tree expects
static uint32_t avl_height(AVLNode *node) {
    return node ? node->height : 0;
}

static uint32_t avl_cnt(AVLNode *node) {
    return node ? node->cnt : 0;
}

// Forward declarations for your AVL functions
AVLNode *avl_fix(AVLNode *node);
AVLNode *avl_del(AVLNode *node);
AVLNode *avl_offset(AVLNode *node, int64_t offset);

// Intrusive wrapper for integer values
struct IntNode {
    AVLNode avl_node;
    int value;
    
    IntNode(int val) : value(val) {}
    
    // Helper to get AVLNode from IntNode
    AVLNode* get_avl_node() { return &avl_node; }
    
    // Helper to get IntNode from AVLNode
    static IntNode* from_avl_node(AVLNode* node) {
        return node ? reinterpret_cast<IntNode*>(
            reinterpret_cast<char*>(node) - offsetof(IntNode, avl_node)
        ) : nullptr;
    }
};

// AVL Tree wrapper class
class MyAVLTree {
private:
    AVLNode* root = nullptr;
    
public:
    ~MyAVLTree() {
        clear();
    }
    
    void clear() {
        clear_recursive(root);
        root = nullptr;
    }
    
    void insert(int value) {
        IntNode* new_node = new IntNode(value);
        root = insert_recursive(root, new_node->get_avl_node(), value);
    }
    
    bool find(int value) {
        return find_recursive(root, value) != nullptr;
    }
    
    bool erase(int value) {
        AVLNode* node = find_recursive(root, value);
        if (!node) return false;
        
        root = avl_del(node);
        delete IntNode::from_avl_node(node);
        return true;
    }
    
    size_t size() {
        return avl_cnt(root);
    }
    
private:
    void clear_recursive(AVLNode* node) {
        if (!node) return;
        clear_recursive(node->left);
        clear_recursive(node->right);
        delete IntNode::from_avl_node(node);
    }
    
    AVLNode* insert_recursive(AVLNode* node, AVLNode* new_node, int value) {
        if (!node) {
            return new_node;
        }
        
        IntNode* current = IntNode::from_avl_node(node);
        if (value < current->value) {
            node->left = insert_recursive(node->left, new_node, value);
            node->left->parent = node;
        } else if (value > current->value) {
            node->right = insert_recursive(node->right, new_node, value);
            node->right->parent = node;
        } else {
            // Duplicate - delete the new node and return current
            delete IntNode::from_avl_node(new_node);
            return node;
        }
        
        // return avl_fix(node);
        AVLNode* new_root = avl_fix(node);
while (new_root && new_root->parent) {
    new_root = new_root->parent;
}
return new_root;
    }
    
    AVLNode* find_recursive(AVLNode* node, int value) {
        if (!node) return nullptr;
        
        IntNode* current = IntNode::from_avl_node(node);
        if (value == current->value) {
            return node;
        } else if (value < current->value) {
            return find_recursive(node->left, value);
        } else {
            return find_recursive(node->right, value);
        }
    }
};

// Test harness
class PerformanceTest {
private:
    std::vector<int> test_data;
    std::vector<int> lookup_data;
    
public:
    PerformanceTest(size_t size) {
        // Generate test data
        test_data.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            test_data.push_back(static_cast<int>(i));
        }
        
        // Shuffle for realistic insertion order
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(test_data.begin(), test_data.end(), g);
        
        // Generate lookup data (mix of existing and non-existing values)
        lookup_data.reserve(size);
        for (size_t i = 0; i < size / 2; ++i) {
            lookup_data.push_back(test_data[i]);  // Existing values
            lookup_data.push_back(static_cast<int>(size + i));  // Non-existing values
        }
        std::shuffle(lookup_data.begin(), lookup_data.end(), g);
    }
    
    void test_std_set() {
        std::cout << "Testing std::set...\n";
        
        std::set<int> set;
        
        // Test insertions
        auto start = std::chrono::high_resolution_clock::now();
        for (int value : test_data) {
            set.insert(value);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto insert_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "std::set insertion time: " << insert_time.count() << " ms\n";
        std::cout << "std::set size: " << set.size() << "\n";
        
        // Test lookups
        start = std::chrono::high_resolution_clock::now();
        size_t found_count = 0;
        for (int value : lookup_data) {
            if (set.find(value) != set.end()) {
                found_count++;
            }
        }
        end = std::chrono::high_resolution_clock::now();
        auto lookup_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "std::set lookup time: " << lookup_time.count() << " ms\n";
        std::cout << "std::set found: " << found_count << " out of " << lookup_data.size() << "\n";
        
        // Test deletions
        start = std::chrono::high_resolution_clock::now();
        for (int value : test_data) {
            set.erase(value);
        }
        end = std::chrono::high_resolution_clock::now();
        auto delete_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "std::set deletion time: " << delete_time.count() << " ms\n";
        std::cout << "std::set final size: " << set.size() << "\n\n";
    }
    
    void test_my_avl() {
        std::cout << "Testing MyAVLTree...\n";
        
        MyAVLTree tree;
        
        // Test insertions
        auto start = std::chrono::high_resolution_clock::now();
        for (int value : test_data) {
            tree.insert(value);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto insert_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "MyAVLTree insertion time: " << insert_time.count() << " ms\n";
        std::cout << "MyAVLTree size: " << tree.size() << "\n";
        
        // Test lookups
        start = std::chrono::high_resolution_clock::now();
        size_t found_count = 0;
        for (int value : lookup_data) {
            if (tree.find(value)) {
                found_count++;
            }
        }
        end = std::chrono::high_resolution_clock::now();
        auto lookup_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "MyAVLTree lookup time: " << lookup_time.count() << " ms\n";
        std::cout << "MyAVLTree found: " << found_count << " out of " << lookup_data.size() << "\n";
        
        // Test deletions
        start = std::chrono::high_resolution_clock::now();
        for (int value : test_data) {
            tree.erase(value);
        }
        end = std::chrono::high_resolution_clock::now();
        auto delete_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "MyAVLTree deletion time: " << delete_time.count() << " ms\n";
        std::cout << "MyAVLTree final size: " << tree.size() << "\n\n";
    }
    
    void run_comparison() {
        std::cout << "=== AVL Tree vs std::set Performance Comparison ===\n";
        std::cout << "Test data size: " << test_data.size() << "\n\n";
        
        test_std_set();
        test_my_avl();
    }
};

int main() {
    // Test with smaller sizes first to debug
    std::vector<size_t> test_sizes = {100, 500, 100};
    
    for (size_t size : test_sizes) {
        PerformanceTest test(size);
        test.run_comparison();
        std::cout << "================================\n\n";
    }
    
    return 0;
}

// FIXED AVL TREE IMPLEMENTATION

static uint32_t max(uint32_t lhs, uint32_t rhs) {
    return lhs < rhs ? rhs : lhs;
}

// maintain the height and cnt field
static void avl_update(AVLNode *node) {
    node->height = 1 + max(avl_height(node->left), avl_height(node->right));
    node->cnt = 1 + avl_cnt(node->left) + avl_cnt(node->right);
}

static AVLNode *rot_left(AVLNode *node) {
    AVLNode *new_node = node->right;
    AVLNode *inner = new_node->left;
    
    // Perform rotation
    node->right = inner;
    new_node->left = node;
    
    // Update parent pointers
    if (inner) {
        inner->parent = node;
    }
    new_node->parent = node->parent;
    node->parent = new_node;
    
    // Update auxiliary data
    avl_update(node);
    avl_update(new_node);
    return new_node;
}

static AVLNode* rot_right(AVLNode* node) {
    AVLNode* new_node = node->left;
    AVLNode* inner = new_node->right;
    
    // Perform rotation
    node->left = inner;
    new_node->right = node;
    
    // Update parent pointers
    if (inner) {
        inner->parent = node;
    }
    new_node->parent = node->parent;
    node->parent = new_node;
    
    // Update auxiliary data
    avl_update(node);
    avl_update(new_node);
    return new_node;
}

static AVLNode *avl_fix_left(AVLNode *node) {
    if (avl_height(node->left->left) < avl_height(node->left->right)) {
        node->left = rot_left(node->left);
    }
    return rot_right(node);
}

static AVLNode *avl_fix_right(AVLNode *node) {
    if (avl_height(node->right->right) < avl_height(node->right->left)) {
        node->right = rot_right(node->right);
    }
    return rot_left(node);
}

// FIXED: Avoid unsigned integer underflow
AVLNode *avl_fix(AVLNode *node) {
    while (node) {
        AVLNode** from = &node;
        AVLNode* parent = node->parent;
        if (parent) {
            from = parent->left == node ? &parent->left : &parent->right;
        }
        
        avl_update(node);
        uint32_t l = avl_height(node->left);
        uint32_t r = avl_height(node->right);
        
        // Use signed comparison to avoid underflow
        int32_t balance = (int32_t)l - (int32_t)r;
        
        if (balance > 1) {
            *from = avl_fix_left(node);
        } else if (balance < -1) {
            *from = avl_fix_right(node);
        }
        
        if (!parent) {
            return *from;
        }
        
        node = parent;
    }
    return nullptr;
}

static AVLNode *avl_del_easy(AVLNode *node) {
    assert(!node->left || !node->right);
    AVLNode *child = node->left ? node->left : node->right;
    AVLNode *parent = node->parent;
    
    if (child) {
        child->parent = parent;
    }
    
    if (!parent) {
        return child;
    }
    
    AVLNode **from = parent->left == node ? &parent->left : &parent->right;
    *from = child;
    
    return avl_fix(parent);
}

AVLNode *avl_del(AVLNode *node) {
    if (!node->left || !node->right) {
        return avl_del_easy(node);
    }
    
    // Find successor
    AVLNode *victim = node->right;
    while (victim->left) {
        victim = victim->left;
    }
    
    // Detach successor
    AVLNode *root = avl_del_easy(victim);
    
    // Replace node with successor
    *victim = *node;
    if (victim->left) {
        victim->left->parent = victim;
    }
    if (victim->right) {
        victim->right->parent = victim;
    }
    
    // Update parent's pointer
    AVLNode **from = &root;
    AVLNode *parent = node->parent;
    if (parent) {
        from = parent->left == node ? &parent->left : &parent->right;
    }
    *from = victim;
    
    return root;
}

static AVLNode* successor(AVLNode* node) {
    if (!node) return nullptr;
    
    if (node->right) {
        node = node->right;
        while (node->left) node = node->left;
        return node;
    }
    
    AVLNode* parent = node->parent;
    while (parent && node == parent->right) {
        node = parent;
        parent = parent->parent;
    }
    return parent;
}

static AVLNode* predecessor(AVLNode* node) {
    if (!node) return nullptr;
    
    if (node->left) {
        node = node->left;
        while (node->right) node = node->right;
        return node;
    }
    
    AVLNode* parent = node->parent;
    while (parent && node == parent->left) {
        node = parent;
        parent = parent->parent;
    }
    return parent;
}

AVLNode *avl_offset(AVLNode *node, int64_t offset) {
    if (!node) return nullptr;
    if (offset == 0) return node;
    
    while (offset > 0 && node) {
        offset--;
        node = successor(node);
    }
    
    while (offset < 0 && node) {
        offset++;
        node = predecessor(node);
    }
    return node;
}