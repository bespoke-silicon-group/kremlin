/**
 * @file kremlin_deque.h
 * @brief Defines a double ended queue.
 */

#ifndef KREMLIN_DEQUE_H
#define KREMLIN_DEQUE_H

typedef struct deque deque;

/**
 * @name Functions
 * @brief A family of functions that operate on a deque.
 *
 * @{
 */

/**
 * Creates a new deque.
 *
 * @memberof			deque
 * @param d             *d will point to the allocated deque.
 * @param copy_func     A function that takes an element and returns a copy. If
 *                      this is NULL, no copy will be performed upon insertion. 
 *                      Otherwise, this data stucture will hold a copy.
 * @param delete_func   A function that deallocates an element. If NULL, no
 *                      deallocations of inserted data will be performed.
 *                      Otherwise, any remaining elements in the data
 *                      structure upon deletion will also be deleted.
 * @return              TRUE on success.
 */
int deque_create(deque** d, void* (*copy_func)(void*), void (*delete_func)(void*));

/**
 * Deletes a deque.
 *
 * @memberof			deque
 * @param d             The deque to delete. *d will be set to NULL.
 * @return              TRUE on success.
 */
int deque_delete(deque** d);

/**
 * Adds an element to the end of the deque.
 *
 * @memberof			deque
 * @param d             The deque.
 * @param element       The data to store.
 * @return              TRUE on success.
 */
int deque_push_back(deque* d, void* element);

/**
 * Adds an element to the front of the deque.
 * @param d             The deque.
 * @param element       The data to store.
 * @return              TRUE on success.
 */
int deque_push_front(deque* d, void* element);

/**
 * Returns the last element.
 *
 * @memberof			deque
 * @param d             The deque.
 * @return              The last element or NULL if no element could be
 *                      retrieved.
 */
void* deque_back(deque* d);

/**
 * Returns the first element.
 *
 * @memberof			deque
 * @param d             The deque.
 * @return              The first element or NULL if no element could be
 *                      retrieved.
 */
void* deque_front(deque* d);

/**
 * Removes and returns the last element.
 *
 * @memberof			deque
 * @param d             The deque.
 * @return              The last element or NULL if no element could be
 *                      retrieved.
 */
void* deque_pop_back(deque* d);

/**
 * Removes and returns the first element.
 *
 * @param d             The deque.
 * @return              The first element or NULL if no element could be
 *                      retrieved.
 */
void* deque_pop_front(deque* d);

/**
 * Removes all elements from the list. If delete_func is not NULL, the
 * elements will also be deallocated with it.
 *
 * @memberof			deque
 * @param d             The deque.
 */
void deque_clear(deque* d);

/**
 * Returns the number of elements currently in the data structure.
 *
 * @memberof			deque
 * @param d             The deque.
 * @return              The number of elements in the data structure.
 */
size_t deque_size(deque* d);

/**
 * @}
 */
#endif /* KREMLIN_DEQUE_H */
