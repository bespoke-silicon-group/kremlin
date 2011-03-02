/**
 * @file Vector.h
 * @brief Generic vector data structure.
 */

#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>
#include <assert.h>

#define VECTOR_DEFAULT_SIZE 16
#define TRUE 1
#define FALSE 0

#define VECTOR_MIN(x,y) (x) < (y) ? (x) : (y)
#define VECTOR_COPY(x,y) *(x) = *(y)
#define VECTOR_NO_DELETE(x) (void)(x)

#define VECTOR_DEFINE_PROTOTYPES(prefix,type) \
    typedef struct prefix prefix; \
    type*   prefix##AtRef(prefix* v, size_t i); \
    type    prefix##AtVal(prefix* v, size_t i); \
    type*   prefix##Begin(prefix* v); \
    int     prefix##Clear(prefix* v); \
    int     prefix##Create(prefix** v); \
    int     prefix##Delete(prefix** v); \
    int     prefix##Empty(prefix* v); \
    type*   prefix##End(prefix* v); \
    type*   prefix##First(prefix* v); \
    type*   prefix##Last(prefix* v); \
    void    prefix##Map(prefix* v, void(*f)(type*)); \
    int     prefix##Pop(prefix* v); \
    type    prefix##PopVal(prefix* v); \
    type*   prefix##Push(prefix* v); \
    int     prefix##PushRef(prefix* v, type*); \
    int     prefix##PushVal(prefix* v, type); \
    int     prefix##Realloc(prefix* v, size_t size); \
    size_t  prefix##Size(prefix* v); \

#define VECTOR_DEFINE_FUNCTIONS(prefix,type,copy,del) \
    struct prefix \
    { \
        size_t dataSize; \
        type* end; \
        type* data; \
    }; \
    \
    type* prefix##AtRef(prefix* v, size_t i) \
    { \
        assert(i < prefix##Size(v)); \
        return v->data + i; \
    } \
    \
    type prefix##AtVal(prefix* v, size_t i) \
    { \
        return *prefix##AtRef(v, i); \
    } \
    \
    type* prefix##Begin(prefix* v) \
    { \
        return v->data; \
    } \
    \
    int prefix##Clear(prefix* v) \
    { \
        type* element; \
        type* end; \
        \
        for(element = prefix##Begin(v), end = prefix##End(v); element < end; element++) \
            del(element); \
        \
        v->end = v->data; \
        \
        return TRUE; \
    } \
    \
    int prefix##Create(prefix** v) \
    { \
        (*v) = (prefix*)calloc(sizeof(prefix), 1); \
        assert(*v); \
        \
        prefix##Realloc(*v, VECTOR_DEFAULT_SIZE); \
        \
        return TRUE; \
    } \
    \
    int prefix##Delete(prefix** v) \
    { \
        prefix##Clear(*v); \
        \
        free((*v)->data); \
        (*v)->data = NULL; \
        \
        free(*v); \
        *v = NULL; \
        \
        return TRUE; \
    } \
    \
    int prefix##Empty(prefix* v) \
    { \
        return prefix##Size(v) == 0; \
    } \
    \
    type* prefix##End(prefix* v) \
    { \
        return v->end; \
    } \
    \
    type* prefix##First(prefix* v) \
    { \
        return prefix##Begin(v); \
    } \
    \
    type* prefix##Last(prefix* v) \
    { \
        return prefix##End(v) - 1; \
    } \
    \
    void prefix##Map(prefix* v, void(*f)(type*)) \
    { \
        type* element; \
        type* end; \
        \
        for(element = prefix##Begin(v), end = prefix##End(v); element < end; element++) \
            f(element); \
        \
    } \
    \
    int prefix##Pop(prefix* v) \
    { \
        assert(v->end > v->data); \
        --v->end; \
        return TRUE; \
    } \
    \
    type prefix##PopVal(prefix* v) \
    { \
        assert(v->end > v->data); \
        return *--v->end; \
    } \
    \
    type* prefix##Push(prefix* v) \
    { \
        if(v->end >= v->data + v->dataSize) \
            prefix##Realloc(v, v->dataSize * 2); \
        \
        return v->end++; \
    } \
    \
    int prefix##PushRef(prefix* v, type* val) \
    { \
        type* dest = prefix##Push(v); \
        copy(dest, val); \
        return TRUE; \
    } \
    \
    int prefix##PushVal(prefix* v, type val) \
    { \
        *prefix##Push(v) = val; \
        return TRUE; \
    } \
    \
    int prefix##Realloc(prefix* v, size_t size) \
    { \
        size_t occupancy = VECTOR_MIN(prefix##Size(v), size); \
        \
        v->data = (type*)realloc(v->data, sizeof(type) * size); \
        assert(v->data); \
        \
        v->dataSize = size; \
        v->end = v->data + occupancy; \
        \
        return TRUE; \
    } \
    \
    size_t prefix##Size(prefix* v) \
    { \
        return v->end - v->data; \
    } \
    \

#endif /* VECTOR_H */
