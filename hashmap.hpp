#pragma once 
#include<bits/stdc++.h>

struct HashNode{
    HashNode* next = nullptr;
    uint64_t hCode = 0;
};

struct HashTable{
    HashNode** table = nullptr;
    size_t mask = 0;
    size_t size = 0;
};

struct HashMap{
    // yes, not pointers just objects.
    HashTable new_table;
    HashTable old_table;

    size_t migrate_pos = 0;
};

HashNode* hmap_lookup (HashMap* hmap , HashNode * key, bool (*eq)(HashNode* , HashNode*));
void hmap_insert(HashMap* hmap , HashNode * node);
HashNode *hmap_delete ( HashMap*hmap, HashNode*key , bool(*eq)(HashNode* , HashNode *));
void hmap_clear(HashMap* hmap);
size_t hmap_size(HashMap *hmap);
void hmap_for_each_key(HashMap* hmap, bool (*f)(HashNode* , void*), void* arg);