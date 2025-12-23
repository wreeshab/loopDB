#include<bits/stdc++.h>
#include"hashmap.hpp"

static void h_init( HashTable *ht , size_t n){
    assert( n > 0 && (n & (n-1)) ==0);

    ht->table = (HashNode**) calloc(n , sizeof( HashNode*));
    ht->size = 0;
    ht->mask = n-1;
}

static void h_insert ( HashTable *htab , HashNode* node){
    size_t pos = node->hCode & htab->mask;
    HashNode* next = htab->table [pos];
    node->next= next;
    htab->table[pos] = node;
    htab->size++;
}

static HashNode **h_lookup(HashTable* htab , HashNode*key , bool(*eq)(HashNode* , HashNode*)){
    if(htab->table == nullptr) return nullptr;

    size_t pos = key->hCode & htab->mask;

    HashNode ** from = &htab->table[pos];
    for(HashNode* curr; (curr = *from )!=nullptr; from = &curr->next){
        if(curr->hCode == key->hCode && eq(curr, key)){
            return from;
        }
    }
    return nullptr;
}

// here from is either htab[hash] or &prev->next so we do not change the address (**) of there we just change the contents of (**) ie. (*).
static HashNode* h_detach(HashTable* htab, HashNode** from){
    HashNode* node = *from;
    *from = node->next;
    htab->size--;
    return node;
}

const size_t k_rehashing_work = 128;

static void hm_help_rehashing(HashMap* hmap){
    size_t work_done = 0;
    while(work_done< k_rehashing_work && hmap->old_table.size>0){
        HashNode ** from = &hmap->old_table.table[hmap->migrate_pos];
        
        if(!*from){ // from points to null instead of pointing to node's pointer.
            hmap->migrate_pos++;
            continue;
        }
        h_insert(&hmap->new_table , h_detach(&hmap->old_table, from));
        work_done++;
    }

    if(hmap->old_table.size == 0 && hmap->old_table.table){
        free(hmap->old_table.table); // table is a double pointer.
        hmap->old_table = HashTable{};
    }
}

static void hm_trigger_rehashing(HashMap* hmap){
    assert(hmap->old_table.table == nullptr); // older table has to be empty.
    hmap->old_table = hmap->new_table;
    h_init(&hmap->new_table, (hmap->new_table.mask+1)*2);
    hmap->migrate_pos = 0;
}

HashNode* hmap_lookup (HashMap* hmap , HashNode * key, bool (*eq)(HashNode* , HashNode*)){
    hm_help_rehashing(hmap);
    HashNode ** from = h_lookup(&hmap->new_table , key , eq);
    if(!from){
        from = h_lookup(&hmap->old_table, key , eq);
    }
    return from ? *from  : nullptr;
}

const size_t k_max_load_factor = 8;

void hm_insert(HashMap* hmap , HashNode * node){
    if(!hmap->new_table.table){
        h_init(&hmap->new_table , 4);
    }
    h_insert(&hmap->new_table, node);

    if(!hmap->old_table.table){
        size_t threshold = (hmap->new_table.mask+1) * k_max_load_factor;
        if(hmap->new_table.size >= threshold){
            hm_trigger_rehashing(hmap);
        }
    }
    hm_help_rehashing(hmap); // migrate some nodes.
}

HashNode *hmap_delete ( HashMap*hmap, HashNode*key , bool(*eq)(HashNode* , HashNode*)){
    hm_help_rehashing(hmap);
    if(HashNode** from = h_lookup(&hmap->new_table , key , eq)){
        return h_detach(&hmap->new_table , from);
    }
    if(HashNode** from = h_lookup(&hmap->old_table , key , eq)){
        return h_detach(&hmap->old_table , from);
    }
    return nullptr;
}

size_t hmap_size(HashMap* hmap){
    return hmap->new_table.size + hmap->old_table.size;
}