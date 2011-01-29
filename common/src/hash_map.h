/**
 * @file hash_map.h
 * @brief Defines a hash map.
 */

#ifndef HASH_MAP_H
#define HASH_MAP_H

typedef struct hash_map hash_map;
typedef unsigned long long hash_map_index;
typedef struct hash_map_iterator hash_map_iterator;
typedef void* hash_map_key;
typedef struct hash_map_node hash_map_node;
typedef void* hash_map_value;

/* Function Prototypes */

/**
 * Removes all elements from the hash map.
 *
 * @param map           The map.
 * @return              TRUE on success.
 */
int hash_map_clear(hash_map* map);

/**
 * Creates a new hash map.
 *
 * @param map           The map. The allocated map will be pointed by *map.
 * @param hash_func     A function that takes a reference to the key and 
 *                      returns an index.
 * @param compare_func  A function that returns TRUE if the two elements are
 *                      equal.
 * @param copy_func     A function that copies an value. If this is not NULL,
 *                      the hash map will store a copy of the value upon
 *                      insertions. If it is NULL, the hash map will store the
 *                      actual value passed.
 * @param delete_func   A function that deallocates a value. If this is NULL,
 *                      the value will not be deallocated upon deleting
 *                      elements from the hash map.
 * @return              TRUE on success.
 */
int hash_map_create(
    hash_map** map, 
    hash_map_index (*hash_func)(hash_map_key), 
    int (*compare_func)(hash_map_key, hash_map_key), 
    hash_map_value (*copy_func)(hash_map_value),
    void (*delete_func)(hash_map_value));

/**
 * Deallocates a hash map.
 * 
 * @param map           The map
 * @return              TRUE on success.
 */
int hash_map_delete(hash_map**);

/**
 * Returns the element stored in the hash map or NULL if the element did not
 * exist.
 *
 * @param map           The map
 * @param key           The key to look up.
 * @return              The value or NULL.
 */
hash_map_value* hash_map_get(hash_map* map, hash_map_key key);

/**
 * Initializes an iterator over the hash map. This will return the first
 * key in the hash map.
 *
 * @param it            The iterator.
 * @param map           The map.
 * @return              The first element in the hash map.
 */
hash_map_key hash_map_it_init(hash_map_iterator* it, hash_map* map);

/**
 * Returns the next element in the iterator or NULL if there are no more
 * elements.
 *
 * @param it            The iterator.
 * @return              The next key in the hash map.
 */
hash_map_key hash_map_it_next(hash_map_iterator* it);

/**
 * Returns the value associated with this key. This does not advance the
 * iterator.
 *
 * @param it            The iterator.
 * @return              The next value in the hash map.
 */
hash_map_value* hash_map_it_value(hash_map_iterator* it);
hash_map_value hash_map_put(hash_map* map, hash_map_key key, hash_map_value value, int force);
hash_map_value hash_map_remove(hash_map* map, hash_map_key key);

/**
 * Reserves at least size entries in the hash map.
 *
 * @param map           The map.
 * @param size          The number of elements to make room for.
 * @return              TRUE on success.
 */
int hash_map_reserve(hash_map* map, size_t size);
int hash_map_set_load(hash_map* map, float load);
hash_map_index hash_map_size(hash_map* map);

/* Should not need to use anything below this line */





















/* Struct definitions */
struct hash_map_node
{
    hash_map_key key;
    hash_map_value value;
    hash_map_node* next;
};

struct hash_map
{
    int (*compare_func)(hash_map_key, hash_map_key);
    hash_map_value (*copy_func)(hash_map_value);
    void (*delete_func)(hash_map_value);
    hash_map_index (*hash_func)(hash_map_key);
    float load;
    hash_map_node** map;
    hash_map_index occupancy;
    hash_map_index size;
};

struct hash_map_iterator
{
    int index;
    hash_map* map;
    hash_map_node* current_node;
    hash_map_node* last_node;
};

/* Crazy macros for more type checking on functions */
#define HASH_MAP_DEFINE_PROTOTYPES(base_name,key_type,value_type) \
    typedef hash_map hash_map_##base_name; \
    typedef hash_map_iterator hash_map_##base_name##_iterator;

#define HASH_MAP_DEFINE_FUNCTIONS(base_name,key_type,value_type) \
    int hash_map_##base_name##_clear(hash_map* map) { return hash_map_clear(map); } \
    int hash_map_##base_name##_create( \
        hash_map** map, \
        hash_map_index (*hash_func)(key_type), \
        int (*compare_func)(key_type, key_type), \
        value_type (*copy_func)(value_type), \
        void (*delete_func)(value_type)) { \
        return hash_map_create( \
            map, \
            (hash_map_index(*)(hash_map_key))hash_func, \
            (int(*)(hash_map_key, hash_map_key))compare_func, \
            (hash_map_value(*)(hash_map_value))copy_func, \
            (void(*)(hash_map_value))delete_func); \
    } \
    int hash_map_##base_name##_delete(hash_map** map) { return hash_map_delete(map); } \
    value_type* hash_map_##base_name##_get(hash_map* map, key_type key) { return (value_type*)hash_map_get(map, (hash_map_key)key); } \
    key_type hash_map_##base_name##_it_init(hash_map_iterator* it, hash_map* map) { return (key_type)hash_map_it_init(it, map); } \
    key_type hash_map_##base_name##_it_next(hash_map_iterator* it) { return (key_type)hash_map_it_next(it); } \
    value_type* hash_map_##base_name##_it_value(hash_map_iterator* it) { return (value_type*)hash_map_it_value(it); } \
    value_type hash_map_##base_name##_put(hash_map* map, key_type key, value_type value, int force) { return (value_type)hash_map_put(map, (hash_map_key)key, (hash_map_value)value, force); } \
    value_type hash_map_##base_name##_remove(hash_map* map, key_type key) { return (value_type)hash_map_remove(map, (hash_map_key)key); } \
    int hash_map_##base_name##_reserve(hash_map* map, hash_map_index size) { return hash_map_reserve(map, size); } \
    int hash_map_##base_name##_set_load(hash_map* map, float load) { return hash_map_set_load(map, load); } \
    hash_map_index hash_map_##base_name##_size(hash_map* map) { return hash_map_size(map); }

#endif /* HASH_MAP_H */
