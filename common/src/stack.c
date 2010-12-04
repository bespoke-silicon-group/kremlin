#include "stack.h"
#include <stdlib.h>

#define TRUE 1
#define FALSE 0
#define DEFAULT_SIZE 16

struct stack
{
	void* (*copy_func)(void*);
	void (*delete_func)(void*);
	int size;
	void** stack;
	void** top;
};

int stack_clear(stack* s)
{
	s->top = s->stack;
	return TRUE;
}

int stack_create(stack** s, void* (*copy_func)(void*), void (*delete_func)(void*))
{
	if(!(*s = (stack*)malloc(sizeof(stack))))
		return FALSE;

	(*s)->copy_func = copy_func;
	(*s)->delete_func = delete_func;
	(*s)->size = DEFAULT_SIZE;
	if(!((*s)->stack = (void**)malloc(sizeof(void*) * DEFAULT_SIZE)))
	{
		free(*s);
		*s = NULL;
		return FALSE;
	}

	(*s)->top = (*s)->stack;
	return TRUE;
}

int stack_delete(stack** s)
{
	void* element;
	if((*s)->delete_func)
		while(element = stack_pop(*s))
			(*(*s)->delete_func)(element);
	free((*s)->stack);
	(*s)->stack = NULL;
	free(*s);
	*s = NULL;
	return TRUE;
}

int stack_is_empty(stack* s)
{
	return stack_size(s) == 0;
}

int stack_push(stack* s, void* element)
{
	if(stack_size(s) >= s->size && !stack_reserve(s, s->size * 2))
		return FALSE;

	if(s->copy_func)
		*s->top++ = (*s->copy_func)(element);
	else
		*s->top++ = element;
	return TRUE;
}

void* stack_pop(stack* s)
{
	if(stack_size(s) <= 0)
		return NULL;
	return *--s->top;
}

void* stack_top(stack* s)
{
	if(stack_size(s) <= 0)
		return NULL;
	return *(s->top - 1);
}

int stack_size(stack* s)
{
	return s->top - s->stack;
}

int stack_reserve(stack* s, int amount)
{
	int occupancy;

	if(amount <= s->size)
		return FALSE;

	occupancy = stack_size(s);
	if(!(s->stack = (void**)realloc(s->stack, sizeof(void*) * amount)))
		return FALSE;

	s->top = s->stack + occupancy;
	s->size = amount;
	return TRUE;
}
