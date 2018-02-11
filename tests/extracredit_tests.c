#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "extracredit.h"
#define NUM_THREADS 10
#define MAP_KEY(kbase, klen) (map_key_t) {.key_base = kbase, .key_len = klen}
#define MAP_VAL(vbase, vlen) (map_val_t) {.val_base = vbase, .val_len = vlen}

hashmap_t *global_map;

typedef struct map_insert_t {
    void *key_ptr;
    void *val_ptr;
} map_insert_t;

/* Used in item destruction */
void map_free_function(map_key_t key, map_val_t val) {
    free(key.key_base);
    free(val.val_base);
}

uint32_t jenkins_hash(map_key_t map_key) {
    const uint8_t *key = map_key.key_base;
    size_t length = map_key.key_len;
    size_t i = 0;
    uint32_t hash = 0;

    while (i != length) {
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }

    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

void map_init(void) {
    global_map = create_map(10, jenkins_hash, map_free_function);
}

void *thread_put(void *arg) {
    map_insert_t *insert = (map_insert_t *) arg;

    put(global_map, MAP_KEY(insert->key_ptr, sizeof(int)), MAP_VAL(insert->val_ptr, sizeof(int)), true);
    return NULL;
}

void map_fini(void) {
    invalidate_map(global_map);
}

Test(map_suite, 00_creation, .timeout = 2, .init = map_init, .fini = map_fini) {
    cr_assert_not_null(global_map, "Map returned was NULL");
}

Test(map_suite, 02_multithreaded, .timeout = 2, .init = map_init, .fini = map_fini) {
    pthread_t thread_ids[10];

    // spawn NUM_THREADS threads to put elements
    for(int index = 0; index < 10; index++) {
        int *key_ptr = malloc(sizeof(int));
        int *val_ptr = malloc(sizeof(int));
        *key_ptr = index;
        *val_ptr = index * 2;

        map_insert_t *insert = malloc(sizeof(map_insert_t));
        insert->key_ptr = key_ptr;
        insert->val_ptr = val_ptr;

        if(pthread_create(&thread_ids[index], NULL, thread_put, insert) != 0)
            exit(EXIT_FAILURE);
    }

    // wait for threads to die before checking queue
    for(int index = 0; index < 10; index++) {
        pthread_join(thread_ids[index], NULL);
    }
    int *key_ptr = malloc(sizeof(int));
    int *val_ptr = malloc(sizeof(int));
    *key_ptr = 33;
    *val_ptr = 100;
    put(global_map, MAP_KEY(key_ptr, 2), MAP_VAL(val_ptr, 3), true);
    put(global_map, MAP_KEY(key_ptr, 2), MAP_VAL(val_ptr, 3), true);
    int *key_ptr2 = malloc(sizeof(int));
    int *val_ptr2 = malloc(sizeof(int));
    *key_ptr2 = 66;
    *val_ptr2 = 200;
    put(global_map, MAP_KEY(key_ptr2, 2), MAP_VAL(val_ptr2, 3), true);
    map_val_t node = get(global_map, MAP_KEY(key_ptr, 2));
    cr_assert_eq(node.val_base, 100, "Had %d items in map. Expected %d", node.val_base, 100);
    node =get(global_map, MAP_KEY(key_ptr2, 2));
    cr_assert_eq(node.val_base, 200, "Had %d items in map. Expected %d", node.val_base, 200);
}
