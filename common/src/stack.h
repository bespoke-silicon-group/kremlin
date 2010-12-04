#ifndef STACK_H
#define STACK_H

typedef struct stack stack;

int stack_create(stack** s, void* (*copy_func)(void*), void (*delete_func)(void*));
int stack_delete(stack** s);

int stack_push(stack* s, void* element);
void* stack_pop(stack* s);
void* stack_top(stack* s);

int stack_size(stack* s);
int stack_is_empty(stack* s);

int stack_clear(stack* s);
int stack_reserve(stack* s, int amount);

#endif /* STACK_H */
