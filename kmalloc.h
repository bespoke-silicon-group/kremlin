#ifndef KMALLOC_H
#define KMALLOC_H

#include <stdlib.h>

/**
 * Allocates a block of memory.
 * @param size The size in bytes to allocate.
 * @return A pointer to the beginning of the allocated space or NULL on error.
 */
void* kmalloc(size_t size);

/**
 * Deallocates a block of memory allocated with kmalloc.
 * @param A pointer returned by kmalloc.
 */
void kfree(void* ptr);

#endif /* KMALLOC_H */
