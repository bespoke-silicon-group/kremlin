#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "hash_map.h"

#define DEFAULT_SIZE 16
#define DEFAULT_LOAD 0.75f
#define TRUE 1
#define FALSE 0

#ifdef DEBUG
#	define IF_DEBUG(level) if(debug >= level)
#else
#	define IF_DEBUG(level) if(0)
#endif /* DEBUG */

enum {
	DEBUG_LEVEL_NONE = 0,
	DEBUG_LEVEL_ERROR,
	DEBUG_LEVEL_WARNING,
	DEBUG_LEVEL_INFO,
	DEBUG_LEVEL_DEBUG
};

static int debug = DEBUG_LEVEL_WARNING;

/* Forward Declarations */
int hash_map_node_create(hash_map_node** node, hash_map_key key, hash_map_value value);
int hash_map_node_delete(hash_map_node** node);
hash_map_index hash_map_get_index(hash_map* map, hash_map_key key);
hash_map_node** hash_map_locate(hash_map* map, hash_map_key key);

/* List of primes right before each power of 2. */
static const unsigned long long primes[] =
{
	1ll, 					3ll, 					7ll, 					13ll, 					31ll, 					61ll, 
	127ll, 					251ll, 					509ll, 					1021ll, 				2039ll, 				4093ll,
	8191ll, 				16381ll, 				32749ll, 				65521ll, 				131071ll, 				262139ll,
	524287ll, 				1048573ll, 				2097143ll, 				4194301ll, 				8388593ll, 				16777213ll,
	33554393ll, 			67108859ll, 			134217689ll, 			268435399ll, 			536870909ll, 			1073741789ll,
	2147483647ll, 			4294967291ll, 			8589934583ll, 			17179869143ll, 			34359738337ll, 			68719476731ll,
	137438953447ll, 		274877906899ll, 		549755813881ll, 		1099511627689ll, 		2199023255531ll, 		4398046511093ll,
	8796093022151ll, 		17592186044399ll, 		35184372088777ll, 		70368744177643ll, 		140737488355213ll, 		281474976710597ll,
	562949953421231ll, 		1125899906842597ll, 	2251799813685119ll, 	4503599627370449ll, 	9007199254740881ll, 	9007199254740881ll,
	18014398509481951ll, 	18014398509481951ll, 	36028797018963913ll, 	36028797018963913ll, 	72057594037927931ll, 	72057594037927931ll,
	144115188075855859ll, 	144115188075855859ll, 	288230376151711717ll, 	288230376151711717ll, 	576460752303423433ll, 	1152921504606846883ll,
	2305843009213693951ll, 	4611686018427387847ll, 	9223372036854775783ll
};

int hash_map_node_create(hash_map_node** node, hash_map_key key, hash_map_value value)
{
	if(!(*node = (hash_map_node*)malloc(sizeof(hash_map_node))))
	{
		IF_DEBUG(DEBUG_LEVEL_ERROR)
			fprintf(stderr, "hash_map_node_create failed because malloc returned NULL\n");
		return FALSE;
	}
	(*node)->key = key;
	(*node)->value = value;
	(*node)->next = NULL;
	return TRUE;
}

int hash_map_node_delete(hash_map_node** node)
{
	free(*node);
	*node = NULL;
	return TRUE;
}

int hash_map_clear(hash_map* map)
{
	hash_map_node** base_element;
	hash_map_node* working_element;
	hash_map_node* next_element;
	for(base_element = map->map; base_element - map->map < map->size; base_element++)
	{
		working_element = *base_element;
		while(working_element)
		{
			next_element = working_element->next;
			if(map->delete_func)
				(*map->delete_func)(working_element->value);

			hash_map_node_delete(&working_element);
			working_element = next_element;
		}
		*base_element = NULL;
	}
	map->occupancy = 0;
	return TRUE;
}

int hash_map_create(
	hash_map** map,
	hash_map_index (*hash_func)(hash_map_key),
	int (*compare_func)(hash_map_key, hash_map_key),
	hash_map_value (*copy_func)(hash_map_value),
	void (*delete_func)(hash_map_value))
{
	if(!(*map = (hash_map*)malloc(sizeof(hash_map))))
	{
		*map = NULL;
		IF_DEBUG(DEBUG_LEVEL_ERROR)
			fprintf(stderr, "hash_map_create - failed because malloc returned NULL\n");
		return FALSE;
	}

	if(!((*map)->map = (hash_map_node**)calloc(DEFAULT_SIZE, sizeof(hash_map_node*))))
	{
		IF_DEBUG(DEBUG_LEVEL_ERROR)
			fprintf(stderr, "hash_map %p - failed during create because calloc returned NULL\n", *map);
		free(*map);
		*map = NULL;
		return FALSE;
	}

	(*map)->compare_func = compare_func;
	(*map)->copy_func = copy_func;
	(*map)->delete_func = delete_func;
	(*map)->hash_func = hash_func;
	(*map)->load = DEFAULT_LOAD;
	(*map)->occupancy = 0;
	(*map)->size = DEFAULT_SIZE;
	if(debug >= DEBUG_LEVEL_INFO)
		fprintf(stderr, "hash_map %p - allocated\n", *map);
	return TRUE;
}

int hash_map_delete(hash_map** map)
{
	IF_DEBUG(DEBUG_LEVEL_INFO)
		fprintf(stderr, "hash_map %p - deallocating\n", *map);

	hash_map_clear(*map);

	free((*map)->map);
	free(*map);
	*map = NULL;
	return TRUE;
}

hash_map_value* hash_map_get(hash_map* map, hash_map_key key)
{
	hash_map_node** found_node;
	if(*(found_node = hash_map_locate(map, key)))
	{

#		ifdef DEBUG
		IF_DEBUG(DEBUG_LEVEL_INFO)
			fprintf(stderr, "hash_map %p - looked up key %p and found %p\n", map, key, (*found_node)->value);
#		endif /* DEBUG */

		return &(*found_node)->value;
	}

#	ifdef DEBUG
	IF_DEBUG(DEBUG_LEVEL_INFO)
		fprintf(stderr, "hash_map %p - looked up key %p, but didn't find\n", map, key);
#	endif /* DEBUG */

	return NULL;
}

hash_map_key hash_map_it_init(hash_map_iterator* it, hash_map* map)
{
	it->map = map;
	it->index = 0;
	it->current_node = NULL;
	it->last_node = NULL;

#	ifdef DEBUG
	IF_DEBUG(DEBUG_LEVEL_DEBUG)
		fprintf(stderr, "hash_map %p - iterator %p - initialized\n", map, it);
#	endif /* DEBUG */

	return hash_map_it_next(it);
}

hash_map_key hash_map_it_next(hash_map_iterator* it)
{
	while(!it->current_node)
		if(it->index < it->map->size)
			it->current_node = it->map->map[it->index++];
		else
		{
			if(debug >= DEBUG_LEVEL_DEBUG)
				fprintf(stderr, "hash_map %p - iterator %p - reached end. Returning NULL\n", it->map, it);
			it->last_node = NULL;
			return NULL;
		}

	it->last_node = it->current_node;
	it->current_node = it->current_node->next;

#	ifdef DEBUG
	IF_DEBUG(DEBUG_LEVEL_DEBUG)
		fprintf(stderr, "hash_map %p - iterator %p - returning key %p\n", it->map, it, it->last_node->key);
#	endif /* DEBUG */

	return it->last_node->key;
}

hash_map_value* hash_map_it_value(hash_map_iterator* it)
{
	return it->index - 1 < it->map->size && it->last_node ? &it->last_node->value : NULL;
}

hash_map_index hash_map_get_index(hash_map* map, hash_map_key key)
{
	return ((*map->hash_func)(key) % map->size);
}

hash_map_node** hash_map_locate(hash_map* map, hash_map_key key)
{
	hash_map_node** working_node;
	for(working_node = map->map + hash_map_get_index(map, key);
		*working_node && !(*map->compare_func)((*working_node)->key, key); 
		working_node = &(*working_node)->next)
    {
#	    ifdef DEBUG
        IF_DEBUG(DEBUG_LEVEL_DEBUG)
            fprintf(stderr, "hash_map %p - locate checking node %p, key: %p, value: %p\n", map, *working_node, (*working_node)->key, (*working_node)->value);
#	    endif /* DEBUG */
    }

#	ifdef DEBUG
	if(debug >= DEBUG_LEVEL_DEBUG)
    {
        if(*working_node)
            fprintf(stderr, "hash_map %p - located key: %p, value: %p at %p\n", map, key, (*working_node)->value, working_node);
        else
            fprintf(stderr, "hash_map %p - did not locate key: %p\n", map, key);
    }
#	endif /* DEBUG */

	return working_node;
}

hash_map_value hash_map_put(hash_map* map, hash_map_key key, hash_map_value value, int force)
{
	hash_map_node** found_node;
	hash_map_value found_value;
	if(*(found_node = hash_map_locate(map, key)))
	{
		if(force)
		{
			found_value = (*found_node)->value;
			(*found_node)->value = value;
			return found_value;
		}
		return (*found_node)->value;
	}
	if(!hash_map_node_create(found_node, key, value))
		return NULL;
	map->occupancy++;

#	ifdef DEBUG
	IF_DEBUG(DEBUG_LEVEL_DEBUG)
		fprintf(stderr, "hash_map %p - added key: %p, value: %p to %p\n", map, key, value, found_node);
#	endif /* DEBUG */

	if((float)map->occupancy / map->size >= map->load)
		hash_map_reserve(map, map->size * 2);
	return value;
}

void* hash_map_remove(hash_map* map, hash_map_key key)
{
	hash_map_value found_value = NULL;
	hash_map_node* last_node = NULL;
	hash_map_node* to_be_deleted;
	hash_map_node** working_node;
	for(working_node = map->map + hash_map_get_index(map, key);
		*working_node && !(*map->compare_func)((*working_node)->key, key); 
		working_node = &(*working_node)->next)
	{
		last_node = *working_node;
	}
	if((to_be_deleted = *working_node))
	{
		if(debug >= DEBUG_LEVEL_DEBUG)
			fprintf(stderr, "hash_map %p - removing key %p with value %p at %p\n", map, to_be_deleted->key, to_be_deleted->value, to_be_deleted);
		found_value = to_be_deleted->value;
		if(last_node)
		{
			last_node->next = (*working_node)->next;
			hash_map_node_delete(&to_be_deleted);
		}
		else
		{
			*working_node = to_be_deleted->next;
			hash_map_node_delete(&to_be_deleted);
		}
		map->occupancy--;
	}
	else
		if(debug >= DEBUG_LEVEL_DEBUG)
			fprintf(stderr, "hash_map %p - failed at removing key %p..returning NULL\n", map, key);
	return found_value;
}

int hash_map_reserve(hash_map* map, size_t size)
{
	hash_map_node** new_map;
	hash_map_node** root_node;
	hash_map_node* link_node;
	hash_map_node** new_spot;
	hash_map_node* next_node;

	if(debug >= DEBUG_LEVEL_INFO)
		fprintf(stderr, "hash_map %p - reserving %llu elements\n", map, (unsigned long long)size);

	if(!(new_map = (hash_map_node**)calloc(size, sizeof(hash_map_node*))))
	{
		if(debug >= DEBUG_LEVEL_ERROR)
			fprintf(stderr, "hash_map %p - reserve of size %llu failed because calloc returned NULL\n", map, (unsigned long long)size);
		return FALSE;
	}

	for(root_node = map->map; root_node - map->map < map->size; ++root_node)
	{
		link_node = *root_node;
		while(link_node)
		{
			for(new_spot = new_map + ((*map->hash_func)(link_node->key) % size); *new_spot; new_spot = &(*new_spot)->next);
			*new_spot = link_node;
			next_node = link_node->next;
			link_node->next = NULL;
			link_node = next_node;
		}
	}

	free(map->map);
	map->map = new_map;
	map->size = size;
	return TRUE;
}

int hash_map_set_load(hash_map* map, float load)
{
	map->load = load;
	return TRUE;
}

hash_map_index hash_map_size(hash_map* map)
{
	return map->occupancy;
}
