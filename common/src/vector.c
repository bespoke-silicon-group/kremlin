#include "vector.h"
#include <stdlib.h>

#define TRUE 1
#define FALSE 0
#define DEFAULT_SIZE 16

struct vector
{
	void* (*copy_func)(void*);
	void (*delete_func)(void*);
	int size;
	void** vector;
	void** top;
};

void* vector_begin(vector* v)
{
	return v->vector;
}

int vector_create(vector** s, void* (*copy_func)(void*), void (*delete_func)(void*))
{
	if(!(*s = (vector*)malloc(sizeof(vector))))
		return FALSE;

	(*s)->copy_func = copy_func;
	(*s)->delete_func = delete_func;
	(*s)->size = DEFAULT_SIZE;
	if(!((*s)->vector = (void**)malloc(sizeof(void*) * DEFAULT_SIZE)))
	{
		free(*s);
		*s = NULL;
		return FALSE;
	}

	(*s)->top = (*s)->vector;
	return TRUE;
}

int vector_delete(vector** s)
{
	void* element;
	if((*s)->delete_func)
		while(element = vector_pop(*s))
			(*(*s)->delete_func)(element);
	free((*s)->vector);
	(*s)->vector = NULL;
	free(*s);
	*s = NULL;
	return TRUE;
}

void* vector_end(vector* v)
{
	return v->top;
}

int vector_push(vector* s, void* element)
{
	if(vector_size(s) >= s->size && !vector_reserve(s, s->size * 2))
		return FALSE;

	if(s->copy_func)
		*s->top++ = (*s->copy_func)(element);
	else
		*s->top++ = element;
	return TRUE;
}

void* vector_pop(vector* s)
{
	if(vector_size(s) <= 0)
		return NULL;
	return *--s->top;
}

void* vector_top(vector* s)
{
	if(vector_size(s) <= 0)
		return NULL;
	return *(s->top - 1);
}

void vector_map(vector* v, void (*func)(void*))
{
    void** it;
    void** end;
    for(it = vector_begin(v), end = vector_end(v); it < end; it++)
        (*func)(*it);
}

int vector_size(vector* s)
{
	return s->top - s->vector;
}

int vector_reserve(vector* s, int amount)
{
	void** realloced_memory;
	int occupancy;

	if(amount <= s->size)
		return FALSE;

	occupancy = vector_size(s);
	if(!(realloced_memory = (void**)realloc(s->vector, sizeof(void*) * amount)))
		return FALSE;
	s->vector = realloced_memory;

	s->top = s->vector + occupancy;
	s->size = amount;
	return TRUE;
}
