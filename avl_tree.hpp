#pragma once
#include <string>
#include <functional>
#include <cstddef>

using namespace std;

// AVL Tree Node structure
struct AVLNode {
    string key;
    string val;
    AVLNode* left;
    AVLNode* right;
    int height;
    
    AVLNode(const string& k, const string& v) 
        : key(k), val(v), left(nullptr), right(nullptr), height(1) {}
};

// AVL Tree class
class AVLTree {
private:
    AVLNode* root;
    size_t node_count;
    
    // Helper functions
    int get_height(AVLNode* node);
    int get_balance_factor(AVLNode* node);
    void update_height(AVLNode* node);
    
    // Rotation operations
    AVLNode* rotate_right(AVLNode* y);
    AVLNode* rotate_left(AVLNode* x);
    
    // Core operations
    AVLNode* insert_helper(AVLNode* node, const string& key, const string& val, bool& updated);
    AVLNode* delete_helper(AVLNode* node, const string& key, bool& deleted);
    AVLNode* find_min(AVLNode* node);
    AVLNode* lookup_helper(AVLNode* node, const string& key);
    
    // Tree traversal
    void inorder_traversal(AVLNode* node, function<void(const string&, const string&)> callback);
    void clear_tree(AVLNode* node);
    
public:
    AVLTree();
    ~AVLTree();
    
    // Public interface
    void insert(const string& key, const string& val);
    bool remove(const string& key);
    string* lookup(const string& key);
    size_t size() const;
    void for_each(function<void(const string&, const string&)> callback);
    void clear();
};
