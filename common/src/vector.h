/**
 * @file vector.h
 * @brief Defines a vector similar to the C++ STL vector.
 */

#ifndef VECTOR_H
#define VECTOR_H

typedef struct vector vector;

int vector_create(vector** v, void* (*copy_func)(void*), void (*delete_func)(void*)) __attribute__ ((deprecated));

/**
 * Creates a new vector.
 *
 * @memberof 			vector
 * @param v             Where to store the newly created vector. *v will point to 
 *                      the vector and should be passed to the other vector 
 *                      functions.
 * @param copy_func     A function to copy an element upon insertion. If 
 *                      copy_func is NULL, the element will be stored by 
 *                      reference.
 * @param delete_func   A function to delete elements upon deletion. If
 *                      delete_func is NULL, the elements will not be deleted.
 * @return              TRUE on success.
 */
int vector_create(vector** v, void* (*copy_func)(void*), void (*delete_func)(void*));

/**
 * Deletes a vector.
 *
 * The vector will be deallocated, any elements in the vector will be deleted
 * if delete_func passed in the constructor is not NULL and *v will be set to
 * NULL.
 *
 * @memberof 			vector
 * @param               A poiner to the pointer to the vector. 
 * @return              TRUE on success.
 */
int vector_delete(vector** v);

/**
 * Puts a element at the end of the vector.
 *
 * @memeberof           vector
 * @param v             The vector.
 * @param element       The element to insert.
 * @return              TRUE on success.
 */
int vector_push(vector* v, void* element);

/**
 * Removes an element from the end of the vector.
 *
 * @memeberof           vector
 * @param v             The vector.
 * @return              The popped element.
 */
void* vector_pop(vector* v);

/**
 * Returns the last element in the vector.
 *
 * @memeberof           vector
 * @param v             The vector.
 * @return              The last element.
 */
void* vector_top(vector* v);

int vector_size(vector* v);

int vector_reserve(vector* v, int amount);

void* vector_begin(vector* v);
void* vector_end(vector* v);

/**
 * Applies func to all elements in the vector.
 *
 * @memeberof           vector
 * @param v             The vector.
 * @param func          The function to apply to all elements of the vector.
 */
void vector_map(vector* v, void (*func)(void*));

#endif /* VECTOR_H */
