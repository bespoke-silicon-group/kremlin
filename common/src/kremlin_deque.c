#include <stdlib.h>
#include <assert.h>
#include "kremlin_deque.h"

#define TRUE 1
#define FALSE 0

/* 
 * TODO: Allocate chunks and keep a start and end pointer on each instead of
 * just using a linked list.
 */

/* --------------------------------------------------------------------------
 * Typedefs (alpha order)
 * ------------------------------------------------------------------------*/

/**
 * @private
 */
typedef struct deque_node deque_node;

/* --------------------------------------------------------------------------
 * Prototypes (by class, then alpha order)
 * ------------------------------------------------------------------------*/
static int deque_node_create(deque_node** n, void* data, void* (*copy_func)(void*), deque_node* next, deque_node* prev);
static int deque_node_delete(deque_node** n);

static int deque_push_first_element(deque* d, void* element);
static void* deque_pop_last_element(deque* d);

/* --------------------------------------------------------------------------
 * Struct defs (alpha order)
 * ------------------------------------------------------------------------*/
struct deque
{
	/**
	 * The front of the deque.
	 *
	 * @private
	 */
    deque_node* head;

	/**
	 * The end of the deque.
	 *
	 * @private
	 */
    deque_node* tail;

	/**
	 * The number of elements in the deque.
	 *
	 * @private
	 */
    size_t occupancy;

	/**
	 * A function to copy elements inserted into the deque.
	 *
	 * @private
	 */
    void* (*copy_func)(void*);

	/**
	 * A function to delete elements in the deque.
	 *
	 * @private
	 */
    void (*delete_func)(void*);
};

/**
 * @private
 */
struct deque_node
{
    void* data;
    deque_node* next;
    deque_node* prev;
};

/* --------------------------------------------------------------------------
 * Function defs (by class, then alpha order)
 * ------------------------------------------------------------------------*/
void* deque_back(deque* d)
{
    return d->tail ? d->tail->data : NULL;
}

void deque_clear(deque* d)
{
    while(d->occupancy > 0)
    {
        void* data = deque_pop_front(d);
        if(d->delete_func)
            (*d->delete_func)(data);
    }
}

int deque_create(deque** d, void* (*copy_func)(void*), void (*delete_func)(void*))
{
    if(!(*d = (deque*)malloc(sizeof(deque))))
    {
        assert(0 && "Failed to alloc deque because malloc returned NULL");
        return FALSE;
    }

    (*d)->head = NULL;
    (*d)->tail = NULL;
    (*d)->occupancy = 0;
    (*d)->copy_func = copy_func;
    (*d)->delete_func = delete_func;

    return TRUE;
}

int deque_delete(deque** d)
{
    deque_clear(*d);
    free(*d);
    *d = NULL;
    return TRUE;
}

void* deque_front(deque* d)
{
    return d->head ? d->head->data : NULL;
}

int deque_push_back(deque* d, void* element)
{
    if(d->occupancy == 0)
        return deque_push_first_element(d, element);

    deque_node* next_tail;
    if(!deque_node_create(&next_tail, element, d->copy_func, NULL, d->tail))
    {
        assert(0 && "Failed to push_back because deque_node_create failed!");
        return FALSE;
    }

    d->tail->next = next_tail;
    d->tail = next_tail;
    d->occupancy++;
    return TRUE;
}

int deque_push_first_element(deque* d, void* element)
{
    assert(d->occupancy == 0);
    assert(d->head == NULL);
    assert(d->tail == NULL);

    if(!deque_node_create(&d->tail, element, d->copy_func, NULL, NULL))
    {
        assert(0 && "Failed to allocate first node because deque_node_create failed!");
        return FALSE;
    }

    d->tail->next = d->tail;
    d->tail->prev = d->tail;
    d->head = d->tail;

    d->occupancy++;

    return TRUE;
}

int deque_push_front(deque* d, void* element)
{
    if(d->occupancy == 0)
        return deque_push_first_element(d, element);

    deque_node* next_head;
    if(!deque_node_create(&next_head, element, d->copy_func, d->head, NULL))
    {
        assert(0 && "Failed to push_back because deque_node_create failed!");
        return FALSE;
    }

    d->head->prev = next_head;
    d->head = next_head;
    d->occupancy++;
    return TRUE;
}

void* deque_pop_back(deque* d)
{
    if(d->occupancy <= 0)
        return NULL;

    if(d->occupancy == 1)
        return deque_pop_last_element(d);

    void* ret = d->tail->data;
    deque_node* new_tail = d->tail->prev;
    
    new_tail->next = NULL;
    deque_node_delete(&d->tail);

    d->tail = new_tail;

    d->occupancy--;

    return ret;
}

void* deque_pop_front(deque* d)
{
    if(d->occupancy <= 0)
        return NULL;

    if(d->occupancy == 1)
        return deque_pop_last_element(d);

    void* ret = d->head->data;
    deque_node* new_head = d->head->next;
    
    new_head->prev = NULL;
    deque_node_delete(&d->head);

    d->head = new_head;

    d->occupancy--;

    return ret;
}

void* deque_pop_last_element(deque* d)
{
    assert(d->occupancy == 1);
    assert(d->head == d->tail);

    void* ret = d->head->data;

    d->tail = NULL;
    deque_node_delete(&d->head);

    d->occupancy--;

    return ret;
}

size_t deque_size(deque* d)
{
    return d->occupancy;
}

int deque_node_create(deque_node** n, void* data, void* (*copy_func)(void*), deque_node* next, deque_node* prev)
{
    if(!(*n = (deque_node*)malloc(sizeof(deque_node))))
    {
        assert(0 && "Failed to alloc deque_node because malloc returned NULL");
        return FALSE;
    }

    if(copy_func)
        data = (*copy_func)(data);
    
    (*n)->data = data;
    (*n)->next = next;
    (*n)->prev = prev;

    return TRUE;
}

int deque_node_delete(deque_node** n)
{
    free(*n);
    *n = NULL;
    return TRUE;
}

