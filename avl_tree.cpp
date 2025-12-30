#include "avl_tree.hpp"
#include <algorithm>

using namespace std;

// Constructor
AVLTree::AVLTree() : root(nullptr), node_count(0) {}

// Destructor
AVLTree::~AVLTree() {
    clear_tree(root);
}

// Get height of a node
int AVLTree::get_height(AVLNode* node) {
    return node ? node->height : 0;
}

// Get balance factor of a node
int AVLTree::get_balance_factor(AVLNode* node) {
    return node ? get_height(node->left) - get_height(node->right) : 0;
}

// Update height of a node
void AVLTree::update_height(AVLNode* node) {
    if (node) {
        node->height = 1 + max(get_height(node->left), get_height(node->right));
    }
}

// Right rotation
AVLNode* AVLTree::rotate_right(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* T2 = x->right;
    
    // Perform rotation
    x->right = y;
    y->left = T2;
    
    // Update heights
    update_height(y);
    update_height(x);
    
    return x;
}

// Left rotation
AVLNode* AVLTree::rotate_left(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* T2 = y->left;
    
    // Perform rotation
    y->left = x;
    x->right = T2;
    
    // Update heights
    update_height(x);
    update_height(y);
    
    return y;
}

// Insert helper with balancing
AVLNode* AVLTree::insert_helper(AVLNode* node, const string& key, const string& val, bool& updated) {
    // Standard BST insert
    if (!node) {
        node_count++;
        updated = false;
        return new AVLNode(key, val);
    }
    
    if (key < node->key) {
        node->left = insert_helper(node->left, key, val, updated);
    } else if (key > node->key) {
        node->right = insert_helper(node->right, key, val, updated);
    } else {
        // Key already exists, update value
        node->val = val;
        updated = true;
        return node;
    }
    
    // Update height
    update_height(node);
    
    // Get balance factor
    int balance = get_balance_factor(node);
    
    // Left-Left case
    if (balance > 1 && key < node->left->key) {
        return rotate_right(node);
    }
    
    // Right-Right case
    if (balance < -1 && key > node->right->key) {
        return rotate_left(node);
    }
    
    // Left-Right case
    if (balance > 1 && key > node->left->key) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }
    
    // Right-Left case
    if (balance < -1 && key < node->right->key) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }
    
    return node;
}

// Find minimum value node
AVLNode* AVLTree::find_min(AVLNode* node) {
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

// Delete helper with rebalancing
AVLNode* AVLTree::delete_helper(AVLNode* node, const string& key, bool& deleted) {
    if (!node) {
        deleted = false;
        return nullptr;
    }
    
    // Standard BST delete
    if (key < node->key) {
        node->left = delete_helper(node->left, key, deleted);
    } else if (key > node->key) {
        node->right = delete_helper(node->right, key, deleted);
    } else {
        // Node to be deleted found
        deleted = true;
        
        // Node with only one child or no child
        if (!node->left || !node->right) {
            node_count--;
            AVLNode* temp = node->left ? node->left : node->right;
            
            // No child case
            if (!temp) {
                temp = node;
                node = nullptr;
            } else {
                // One child case
                *node = *temp;
            }
            delete temp;
        } else {
            // Node with two children
            AVLNode* temp = find_min(node->right);
            node->key = temp->key;
            node->val = temp->val;
            node->right = delete_helper(node->right, temp->key, deleted);
            deleted = true;
        }
    }
    
    if (!node) {
        return nullptr;
    }
    
    // Update height
    update_height(node);
    
    // Get balance factor
    int balance = get_balance_factor(node);
    
    // Left-Left case
    if (balance > 1 && get_balance_factor(node->left) >= 0) {
        return rotate_right(node);
    }
    
    // Left-Right case
    if (balance > 1 && get_balance_factor(node->left) < 0) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }
    
    // Right-Right case
    if (balance < -1 && get_balance_factor(node->right) <= 0) {
        return rotate_left(node);
    }
    
    // Right-Left case
    if (balance < -1 && get_balance_factor(node->right) > 0) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }
    
    return node;
}

// Lookup helper
AVLNode* AVLTree::lookup_helper(AVLNode* node, const string& key) {
    if (!node) {
        return nullptr;
    }
    
    if (key == node->key) {
        return node;
    } else if (key < node->key) {
        return lookup_helper(node->left, key);
    } else {
        return lookup_helper(node->right, key);
    }
}

// In-order traversal
void AVLTree::inorder_traversal(AVLNode* node, function<void(const string&, const string&)> callback) {
    if (node) {
        inorder_traversal(node->left, callback);
        callback(node->key, node->val);
        inorder_traversal(node->right, callback);
    }
}

// Clear tree
void AVLTree::clear_tree(AVLNode* node) {
    if (node) {
        clear_tree(node->left);
        clear_tree(node->right);
        delete node;
    }
}

// Public insert
void AVLTree::insert(const string& key, const string& val) {
    bool updated = false;
    root = insert_helper(root, key, val, updated);
}

// Public remove
bool AVLTree::remove(const string& key) {
    bool deleted = false;
    root = delete_helper(root, key, deleted);
    return deleted;
}

// Public lookup
string* AVLTree::lookup(const string& key) {
    AVLNode* node = lookup_helper(root, key);
    return node ? &node->val : nullptr;
}

// Get size
size_t AVLTree::size() const {
    return node_count;
}

// For each key-value pair
void AVLTree::for_each(function<void(const string&, const string&)> callback) {
    inorder_traversal(root, callback);
}

// Clear all nodes
void AVLTree::clear() {
    clear_tree(root);
    root = nullptr;
    node_count = 0;
}
