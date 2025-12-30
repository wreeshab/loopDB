# AVL Tree Implementation for LoopDB

This document describes the AVL tree implementation added to LoopDB.

## Overview

An AVL tree is a self-balancing binary search tree where the heights of the two child subtrees of any node differ by at most one. This implementation provides efficient O(log n) time complexity for insert, delete, and search operations.

## Features

### Core Operations

1. **Insert (`avl_set key value`)**: Inserts a new key-value pair into the AVL tree or updates an existing key
   - Time Complexity: O(log n)
   - Automatically balances the tree using rotations

2. **Search (`avl_get key`)**: Retrieves the value associated with a key
   - Time Complexity: O(log n)
   - Returns (nil) if key doesn't exist

3. **Delete (`avl_del key`)**: Removes a key-value pair from the tree
   - Time Complexity: O(log n)
   - Rebalances the tree after deletion

4. **List Keys (`avl_keys`)**: Returns all keys in sorted order
   - Time Complexity: O(n)
   - Uses in-order traversal

### Balancing Mechanism

The AVL tree maintains balance using four types of rotations:

1. **Left-Left (LL)**: Single right rotation
2. **Right-Right (RR)**: Single left rotation
3. **Left-Right (LR)**: Left rotation followed by right rotation
4. **Right-Left (RL)**: Right rotation followed by left rotation

## Implementation Details

### Files

- `avl_tree.hpp`: Header file with AVL node and tree class definitions
- `avl_tree.cpp`: Implementation of all AVL tree operations
- `server.cpp`: Integration with the LoopDB server (added avl_* commands)

### Data Structure

```cpp
struct AVLNode {
    string key;
    string val;
    AVLNode* left;
    AVLNode* right;
    int height;
};

class AVLTree {
    // Public interface: insert, remove, lookup, size, for_each, clear
    // Private helpers: rotations, balancing, traversal
};
```

### Key Properties

1. **Sorted Order**: Keys are maintained in alphabetical order
2. **Balanced Height**: Tree height is always O(log n)
3. **Self-Balancing**: Automatic rebalancing after every insert/delete
4. **Efficient Operations**: All operations run in O(log n) time

## Usage Examples

### Basic Operations

```bash
# Insert key-value pairs
avl_set alice 100
avl_set bob 200
avl_set charlie 300

# Retrieve values
avl_get alice
# Returns: "100"

# List all keys (in sorted order)
avl_keys
# Returns: ["alice", "bob", "charlie"]

# Update existing key
avl_set alice 999
avl_get alice
# Returns: "999"

# Delete a key
avl_del bob
avl_keys
# Returns: ["alice", "charlie"]
```

### Integration with HashMap

LoopDB maintains two separate storage systems:

1. **HashMap** (original): Uses `get`, `set`, `del`, `keys` commands
2. **AVL Tree** (new): Uses `avl_get`, `avl_set`, `avl_del`, `avl_keys` commands

Both storage systems operate independently:

```bash
# HashMap operations
set key1 value1
keys
# Returns: ["key1"]

# AVL Tree operations
avl_set key2 value2
avl_keys
# Returns: ["key2"]

# Separate storage - no overlap
keys
# Returns: ["key1"]
```

## Performance Characteristics

| Operation | Time Complexity | Space Complexity |
|-----------|----------------|------------------|
| Insert    | O(log n)       | O(1)             |
| Search    | O(log n)       | O(1)             |
| Delete    | O(log n)       | O(1)             |
| List Keys | O(n)           | O(1)             |
| Tree Height | Always ≤ 1.44 log(n) | - |

## Advantages over HashMap

1. **Sorted Order**: Keys are maintained in sorted order, useful for range queries
2. **Predictable Performance**: O(log n) worst-case time complexity (HashMap can degrade)
3. **No Rehashing**: AVL tree doesn't need expensive rehashing operations
4. **Better for Ordered Data**: Ideal when you need to iterate keys in order

## Testing

Run the client and test AVL operations:

```bash
./server &
./client

# Try various operations
> avl_set zebra 26
> avl_set apple 1
> avl_set mango 13
> avl_keys
# Should return: ["apple", "mango", "zebra"]
```

## Future Enhancements

Potential improvements for the AVL tree:

1. Range queries (get all keys between k1 and k2)
2. Predecessor/successor operations
3. Rank queries (find k-th smallest element)
4. Persistence (save/load AVL tree to/from disk)
5. Concurrent access support (thread-safe operations)
