#include "hw5.h"

void rdLock(hashmap_t *self){
    pthread_mutex_lock(&self->fields_lock);
    self->num_readers += 1;
    if(self->num_readers == 1){
        pthread_mutex_lock(&self->write_lock);
    }
    pthread_mutex_unlock(&self->fields_lock);
}

void finLock(hashmap_t *self){
    pthread_mutex_lock(&self->fields_lock);
    self->num_readers -= 1;
    if(self->num_readers == 0){
        pthread_mutex_unlock(&self->write_lock);
    }
    pthread_mutex_unlock(&self->fields_lock);

}

map_node_t* searchMap(hashmap_t *self, map_key_t key){
    map_node_t *node;
    int index = get_index(self, key);
    node = ((self->nodes) + index);
    int i = 0;
    bool full = false;
    while(node->key.key_base == NULL){
        node = ((self->nodes) + index + i);
        i++;
        if((i + index) >= self->capacity && !full){
            index = 0;
            i = 0;
            full = true;
        }else if((i + index) >= self->capacity && full){
            node = NULL;
            return node;
        }
    }

    while(strcmp(node->key.key_base, key.key_base) != 0){
        node = ((self->nodes) + index + i);
        i++;
        if((i + index) >= self->capacity && !full){
            index = 0;
            i = 0;
            full = true;
        }else if((i + index) >= self->capacity && full){
            node = NULL;
            return node;
        }
        while(node->key.key_base == NULL){
            node = ((self->nodes) + index + i);
            i++;
            if((i + index) >= self->capacity && !full){
                index = 0;
                i = 0;
                full = true;
            }else if((i + index) >= self->capacity && full){
                node = NULL;
                return node;
            }
        }
    }


    return node;
}

hashmap_t *create_map(uint32_t capacity, hash_func_f hash_function, destructor_f destroy_function) {
    hashmap_t *hm = calloc(1, sizeof(hashmap_t));

    if(hm == NULL){
        free(hm);
        return NULL;
    }

    hm->capacity = capacity;
    hm->hash_function = hash_function;
    hm->destroy_function = destroy_function;
    hm->size = 0;
    hm->num_readers = 0;
    hm->invalid = false;
    pthread_mutex_t wl;// = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_t fl;// = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(&wl, NULL);
    pthread_mutex_init(&fl, NULL);
    hm->write_lock = wl;
    hm->fields_lock = fl;
    hm->lruHead = malloc(sizeof(map_lru_t));
    hm->lruFoot = hm->lruHead;
    map_node_t *node;
    hm->nodes = calloc(hm->capacity, sizeof(map_node_t));
    for(int i = 0; i < capacity; i++){
        node = ((hm->nodes) + i);
        node->tombstone = true;
    }

    return hm;
}

bool put(hashmap_t *self, map_key_t key, map_val_t val, bool force) {
    if(self == NULL){
        errno = EINVAL;
        return false;
    }
    rdLock(self);
    if(self->invalid || key.key_base == NULL || val.val_base == NULL){
        errno = EINVAL;
        self->destroy_function(key, val);
        finLock(self);
        return false;
    }

    if(self->size == self->capacity){
        if(!force && searchMap(self, key) == NULL){
            errno = ENOMEM;
            self->destroy_function(key, val);
            finLock(self);
            return false;
        }
    }
    int index = get_index(self, key);

    finLock(self);
    pthread_mutex_lock(&self->write_lock);

    map_node_t *node = (self->nodes) + index;
    if(!node->tombstone && force && self->size == self->capacity){
        self->lruHead->node = node;
        self->lruHead = self->lruHead->next;
        free(self->lruHead->prev);
        self->lruHead->prev = NULL;
    }else if(!node->tombstone && !force && self->size == self->capacity){
        if(&(node->key) != &key){
            /*pthread_mutex_unlock(&self->write_lock);
            rdLock(self);*/
            node = searchMap(self, key);
            /*finLock(self);
            pthread_mutex_lock(&self->write_lock);*/
            if(node == NULL){
                errno = ENOMEM;
                self->destroy_function(key, val);
                pthread_mutex_unlock(&self->write_lock);
                return false;
            }

            self->destroy_function(node->key, node->val);

        }
    }else if(!node->tombstone && self->size < self->capacity){
        int i = 0;
            node = searchMap(self, key);
            if(node == NULL){
                node = (self->nodes) + index;
                while(!node->tombstone){
                    node = ((self->nodes) + index + i);
                    i++;
                    if((i + index) >= self->capacity){
                        index = 0;
                        i = 0;
                    }
                }
            }else{
                self->destroy_function(node->key, node->val);
            }
    }
    map_lru_t *lru = malloc(sizeof(map_lru_t));
    map_lru_t *find = NULL;
    if(self->lruHead != NULL){
        find = self->lruHead;
    }
    while(find != NULL && find->node != node){
        find = find->next;
    }
    if(find != NULL){
        node->key.key_base = key.key_base;
        node->key.key_len = key.key_len;
        node->val.val_base = val.val_base;
        node->val.val_len = val.val_len;
        if(find != self->lruFoot){
            find->next->prev = find->prev;
            find->prev->next = find->next;
            self->lruFoot->next = find;
            find->prev = self->lruFoot;
            self->lruFoot = find;
        }
        free(lru);
    }else{
        lru->node = node;
        self->lruFoot->next = lru;
        lru->prev = self->lruFoot;
        self->lruFoot = lru;
    }
    if(node->tombstone){
        self->size += 1;
    }
    node->tombstone = false;

    pthread_mutex_unlock(&self->write_lock);

    return true;
}

map_val_t get(hashmap_t *self, map_key_t key) {
    if(self == NULL){
        errno = EINVAL;
        return MAP_VAL(NULL, 0);
    }
    rdLock(self);
    if(self->invalid == true || key.key_len == 0 || key.key_base == NULL){
        errno = EINVAL;
        finLock(self);
        return MAP_VAL(NULL, 0);
    }

    map_node_t *node = searchMap(self, key);
    if(node == NULL){
        map_val_t map;
        map.val_base = NULL;
        map.val_len = 0;
        errno = EINVAL;
        finLock(self);
        return map;
    }

    map_lru_t *find = self->lruHead;
    while(find != NULL && find->node != node){
        find = find->next;
    }
    if(self->lruFoot != find){
        self->lruFoot->next = find;
        find->prev = self->lruFoot;
        self->lruFoot = find;
    }
    finLock(self);
    return node->val;
}

map_node_t delete(hashmap_t *self, map_key_t key) {
    if(self == NULL){
        map_node_t node;
        node.key.key_base = NULL;
        node.key.key_len = 0;
        node.val.val_base = NULL;
        node.val.val_len = 0;
        node.tombstone = true;
        errno = EINVAL;
        return node;
    }
    rdLock(self);
    if(self->invalid || key.key_base  == NULL || key.key_len == 0){
        map_node_t node;
        node.key.key_base = NULL;
        node.key.key_len = 0;
        node.val.val_base = NULL;
        node.val.val_len = 0;
        node.tombstone = true;
        errno = EINVAL;
        finLock(self);
        return node;
    }
    map_node_t *node = searchMap(self, key);
    finLock(self);
    pthread_mutex_lock(&self->write_lock);
    if(node == NULL){
        map_node_t node;
        node.key.key_base = NULL;
        node.key.key_len = 0;
        node.val.val_base = NULL;
        node.val.val_len = 0;
        node.tombstone = true;
        errno = EINVAL;
        pthread_mutex_unlock(&self->write_lock);
        return node;
    }
    self->destroy_function(node->key, node->val);
    node->key.key_base = NULL;
    node->key.key_len = 0;
    node->tombstone = true;
    self->size -= 1;
    pthread_mutex_unlock(&self->write_lock);
    return *node;
}

bool clear_map(hashmap_t *self) {
    if(self == NULL){
        errno = EINVAL;
        return false;
    }
    rdLock(self);
    if(self->invalid){
        errno = EINVAL;
        finLock(self);
        return false;
    }
    finLock(self);
    pthread_mutex_lock(&self->write_lock);
    map_node_t *node;
    for(int i = 0; i < self->capacity; i++){
        node = ((self->nodes) + i);
        if(!node->tombstone){
            node->tombstone = true;
            self->destroy_function(node->key,node->val);
            node->key.key_base = NULL;
            node->key.key_len = 0;
            node->val.val_base = NULL;
            node->val.val_len = 0;
        }
    }
    self->size = 0;
    pthread_mutex_unlock(&self->write_lock);
    return true;
}

bool invalidate_map(hashmap_t *self) {
    if(self == NULL){
        errno = EINVAL;
        return false;
    }
    rdLock(self);
    if(self->invalid){
        errno = EINVAL;
        finLock(self);
        return false;
    }
    finLock(self);
    pthread_mutex_lock(&self->write_lock);
    map_node_t *node;
    for(int i = 0; i < self->capacity; i++){
        node = ((self->nodes) + i);
        if(!node->tombstone){
            node->tombstone = true;
            self->destroy_function(node->key,node->val);
        }
    }
    free(self->nodes);
    self->invalid = true;
    pthread_mutex_unlock(&self->write_lock);
    return true;
}