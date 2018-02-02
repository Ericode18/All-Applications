#include "utils.h"
#include "errno.h"
#include "debug.h"
#include <string.h>
#include "csapp.h"

#define MAP_KEY(base, len) (map_key_t) {.key_base = base, .key_len = len}
#define MAP_VAL(base, len) (map_val_t) {.val_base = base, .val_len = len}
#define MAP_NODE(key_arg, val_arg, tombstone_arg) (map_node_t) {.key = key_arg, .val = val_arg, .tombstone = tombstone_arg}

/*
 * This function will calloc(3) a new instance of hashmap_t
 *      that manages an array of capacity map_node_t instances
 *
 * @param capacity The maximum number of items that the map can hold.
 * @param hash_function The hash function that the map uses to hash keys.
 * @param destroy_function The destroyer function that the map uses to free keys and values when it is destroyed.
 *
 * @returns A valid pointer to a hashmap_t instance, or NULL.
 *
 * Error case: If any parameters are invalid, set errno to EINVAL and return NULL.
 * Error case: If calloc(3) is unsuccessful or any of the locks cannot be initialized, return NULL.
 */
hashmap_t *create_map(uint32_t capacity, hash_func_f hash_function, destructor_f destroy_function) {
    if(hash_function == NULL || destroy_function == NULL || capacity == 0) {
        errno = EINVAL;
        return NULL;
    }
    // create hashmap and check if calloc failed
    struct hashmap_t *hashmap = (struct hashmap_t*) calloc(1, sizeof(hashmap_t));
    if(hashmap == NULL) {
        return NULL;
    }
    hashmap->capacity = capacity;
    hashmap->hash_function = hash_function;
    hashmap->destroy_function = destroy_function;
    hashmap->nodes = (struct map_node_t*) calloc(hashmap->capacity, sizeof(map_node_t));
    if (hashmap->nodes == NULL) {
        free(hashmap);
        return NULL;
    }

    int x = pthread_mutex_init(&hashmap->write_lock, NULL);
    int y = pthread_mutex_init(&hashmap->fields_lock, NULL);
    if (x != 0 || y != 0) {
        free(hashmap);
        return NULL;
    }

    int i = 0;
    while(i < capacity) {
        hashmap->nodes[i].tombstone = false;
        i++;
    }

    return hashmap;
}

/*
 * This will insert a key/value pair into the hashmap pointed to by self.
 *
 * @param self A pointer to the hashmap
 * @param key The key associated with the node
 * @param val The value associated with the node
 * @param force If the map is full and force is true, overwrite the entry at the index given by get_index and return true.
 *
 * @returns true if the operation was successful, false otherwise.
 *
 * Error case: If any parameters are invalid, set errno to EINVAL and return NULL.
 * Error case: If the map is full and force is set to false, set errno to ENOMEM and return false.
 */
bool put(hashmap_t *self, map_key_t key, map_val_t val, bool force) {

    if (self == NULL || key.key_base == NULL || key.key_len == 0 || val.val_base == NULL || val.val_len == 0 || self->invalid) {
        errno = EINVAL;
        return false;
    }

    pthread_mutex_lock(&self->write_lock);
    debug("capacity %d", self->capacity);
    debug("SIZE: %d", self->size);

    if (self->size >= self->capacity && force == false) {
        errno = ENOMEM;
        return false;
    }
    // set nodes at position get_index and return
    if(self->size >= self->capacity && force == true) {
        // LOCK HERE
        int i = get_index(self, key);
        debug("@@@PUT INDEX %d", i);
        //self->destroy_function(self->nodes[get_index(self, key)].key, self->nodes[get_index(self, key)].val);
        self->nodes[i].key.key_base = key.key_base;
        self->nodes[i].key.key_len = key.key_len;
        self->nodes[i].val.val_base = val.val_base;
        self->nodes[i].val.val_len = val.val_len;
        self->nodes[i].tombstone = false;
        //UNLOCK HERE
        pthread_mutex_unlock(&self->write_lock);

        return true;
    }


    // check if key is in hash map
    // if it is, set the value
    int nodeIndex = get_index(self, key);
    int i = nodeIndex;
    debug("PUT INDEX %d", i);
    map_node_t *myMapNode;
    while(i < self->capacity+nodeIndex) {
        myMapNode = &self->nodes[i % self->capacity];
        debug("Store: %d", i % self->capacity);
        if(myMapNode == NULL || myMapNode->key.key_base == NULL || myMapNode->key.key_len == 0 || myMapNode->val.val_base == NULL
            || myMapNode->val.val_len == 0) {
            debug("here1");
            myMapNode->key.key_base = key.key_base;
            myMapNode->key.key_len = key.key_len;
            myMapNode->val.val_base = val.val_base;
            myMapNode->val.val_len = val.val_len;
            myMapNode->tombstone = false;
            self->size++;
            pthread_mutex_unlock(&self->write_lock);

            return true;
            // check if equal
        } else if (myMapNode->tombstone == true || (key.key_len == myMapNode->key.key_len &&
            memcmp(key.key_base, myMapNode->key.key_base, key.key_len) == 0) || (key.key_base == NULL && myMapNode == NULL)) {
            debug("Here2");
            if (!(myMapNode == NULL || myMapNode->key.key_base == NULL || myMapNode->key.key_len == 0 || myMapNode->val.val_base == NULL
            || myMapNode->val.val_len == 0)) {
                debug("equal but both null");
                self->destroy_function(myMapNode->key, myMapNode->val);
                myMapNode->key.key_base = key.key_base;
                myMapNode->key.key_len = key.key_len;
                myMapNode->val.val_base = val.val_base;
                myMapNode->val.val_len = val.val_len;
                myMapNode->tombstone = false;
                self->size++;
                pthread_mutex_unlock(&self->write_lock);
                return true;
            }
        }
        i++;
    }

    debug("ERRORRRRR");

    return false;
}

/*
 * Retrieves the map_val_t corresponding to key
 *
 * @param self A pointer to the hashmap
 * @param key The key associated with the node.
 *
 * @returns The corresponding value. If the key is not found,
 *           the map_val_t instance will contain a NULL pointer and a val_len of 0.
 *
 * Error case: If any parameters are invalid, set errno to EINVAL and return NULL.
 * Error case: The returned map_val_t instance should contain the same fields as tje case where key is not found
 */
map_val_t get(hashmap_t *self, map_key_t key) {
    if (self == NULL || key.key_len == 0 || self->invalid) {
        errno = EINVAL;
        return MAP_VAL(NULL, 0);
    }
    debug("111");
    pthread_mutex_lock(&self->fields_lock);
    debug("here");
    self->num_readers++;
    if (self->num_readers == 1) {
        pthread_mutex_lock(&self->write_lock);
    }
    pthread_mutex_unlock(&self->fields_lock);
    debug("GET KEY %s", (char*)key.key_base);
    int nodeIndex = get_index(self, key);
    debug("Node Index: %d", nodeIndex);
    int i = nodeIndex;
    map_node_t *myMapNode;
    while(i < self->capacity+nodeIndex) {
        debug("I: %d", i);
        myMapNode = &self->nodes[i % self->capacity];
        debug("Tomb %d", myMapNode->tombstone);
        debug("KEY: %s", (char*)myMapNode->key.key_base);
        // found!!!
        if (self->nodes[i].tombstone == false && ((key.key_len == myMapNode->key.key_len &&
            memcmp(key.key_base, myMapNode->key.key_base, key.key_len) == 0) || (key.key_base == NULL && myMapNode == NULL))) {
            debug("MEMCMP: %s", (char*)key.key_base);
            debug("MEMCMP: %s", (char*)myMapNode->key.key_base);
            debug("KEY LEN: %d", (int)key.key_len);
            break;
        }
        i++;
        myMapNode = NULL;
    }

    pthread_mutex_lock(&self->fields_lock);
    self->num_readers--;
    if(self->num_readers == 0) {
        pthread_mutex_unlock(&self->write_lock);
    }
    pthread_mutex_unlock(&self->fields_lock);

    if(myMapNode != NULL) {
        return myMapNode->val;
    }

    // not found
    return MAP_VAL(NULL, 0);
}

/*
 * Removes the entry with key
 *
 * @param self A pointer to the hashmap
 * @param key The key associated with the node.
 *
 * @returns The removed map_node_t instance.
 *
 * Error case: If any parameters are invalid, set errno to EINVAL and
 *             return a map_node_t with all pointers set to NULL and lengths set to 0.
 */
map_node_t delete(hashmap_t *self, map_key_t key) {
    if (self == NULL || key.key_base == NULL || key.key_len == 0 || self->invalid) {
        errno = EINVAL;
        return MAP_NODE(MAP_KEY(NULL, 0), MAP_VAL(NULL, 0), false);
    }
    // lock write thread before we write
    pthread_mutex_lock(&self->write_lock);

    int nodeIndex = get_index(self, key);
    int i = nodeIndex;
    map_node_t *myMapNode;
    while(i < self->capacity+nodeIndex) {
        myMapNode = &self->nodes[i % self->capacity];
        if (self->nodes[i].tombstone == false && ((key.key_len == myMapNode->key.key_len &&
            memcmp(key.key_base, myMapNode->key.key_base, key.key_len) == 0) || (key.key_base == NULL && myMapNode == NULL))) {
            debug("BREAKKKKK");
            break;
        }
        i++;
        myMapNode = NULL;
    }

    if (!(myMapNode == NULL || myMapNode->key.key_base == NULL || myMapNode->key.key_len == 0 || myMapNode->val.val_base == NULL
            || myMapNode->val.val_len == 0)) {
        debug("TOMB %s", (char*) myMapNode->key.key_base);
        myMapNode->tombstone = true;
        self->size--;
    }
    // unlock write thread when we finished
    pthread_mutex_unlock(&self->write_lock);

    if(myMapNode != NULL) {
        return *myMapNode;
    }

    return MAP_NODE(MAP_KEY(NULL, 0), MAP_VAL(NULL, 0), false);
}

/*
 * Clears all remaining entries in the map. It will call the destroy_function in self on every remaining item.
 *
 * @param self A pointer to the hashmap
 *
 * @returns true if the operation was successful, false otherwise.
 *
 * Error case: If any parameters are invalid, set errno to EINVAL and return false.
 */
bool clear_map(hashmap_t *self) {
    if(self == NULL || self->invalid) {
        debug("FAILED");
        errno = EINVAL;
        return false;
    }
    // lock write thread before we write
    pthread_mutex_lock(&self->write_lock);
    int i = 0;
    while(i < self->capacity) {
        self->destroy_function(self->nodes[i].key, self->nodes[i].val);
        self->nodes[i].key.key_base = NULL;
        self->nodes[i].key.key_len = 0;
        self->nodes[i].val.val_base = NULL;
        self->nodes[i].val.val_len = 0;
        i++;
    }

    self->size = 0;
    // unlock write thread after we finished writing
    pthread_mutex_unlock(&self->write_lock);
    debug("TRUE");
	return true;
}

/*
 * This will invalidate the hashmap_t instances pointed to by self. It will call the destroy function in self on every remaining item.
 * It will free(3) the nodes pointer in self. It will set the invalid flag to true.
 *
 * @param self A pointer to the hashmap
 *
 * @returns true if the invalidation was successful, false otherwise.
 *
 * Error case: If any parameters are invalid, set errno to EINVAL and return false.
 */
bool invalidate_map(hashmap_t *self) {
    if(self == NULL || self->invalid) {
        errno = EINVAL;
        return false;
    }
    // lock write thread before we write
    pthread_mutex_lock(&self->write_lock);

    int counter = self->capacity;
    int i = 0;
    while(counter > 0) {
        self->destroy_function(self->nodes[i].key, self->nodes[i].val);
        self->nodes[i].key.key_base = NULL;
        self->nodes[i].key.key_len = 0;
        self->nodes[i].val.val_base = NULL;
        self->nodes[i].val.val_len = 0;
        counter--;
        i++;
    }

    self->size = 0;
    self->invalid = true;
    free(self->nodes);
    pthread_mutex_unlock(&self->write_lock);

    return true;
}
