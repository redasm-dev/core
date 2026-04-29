#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef int (*VectCompare)(const void*, const void*);
typedef bool (*VectPredicate)(const void*);

#define vect_length(self) ((self)->length)
#define vect_capacity(self) ((self)->capacity)
#define vect_is_empty(self) ((self)->length == 0)

#define vect_push(self, ...)                                                   \
    do {                                                                       \
        _vect_grow((void**)(&(self)->data), &(self)->capacity, (self)->length, \
                   sizeof(*(self)->data));                                     \
        (self)->data[(self)->length++] = __VA_ARGS__;                          \
    } while(0)

#define vect_destroy(self)                                                     \
    do {                                                                       \
        free((void*)((self)->data));                                           \
        (self)->capacity = 0;                                                  \
        (self)->length = 0;                                                    \
        (self)->data = NULL;                                                   \
    } while(0)

#define vect_each(it, self)                                                    \
    for((it) = (self)->data;                                                   \
        (self)->data && ((it) < (self)->data + (self)->length); (it)++)

#define vect_clear(self) (self)->length = 0

#define vect_remove(self, it, n)                                               \
    do {                                                                       \
        assert((it) >= (self)->data &&                                         \
               (it) + (n) <= (self)->data + (self)->length &&                  \
               "vect_remove: out of bounds");                                  \
        memmove((it), (it) + (n),                                              \
                ((self)->data + (self)->length - (it) - (n)) *                 \
                    sizeof(*(self)->data));                                    \
        (self)->length -= (n);                                                 \
    } while(0)

#define vect_remove_at(self, i, n)                                             \
    do {                                                                       \
        assert((i) + (n) <= (self)->length &&                                  \
               "vect_remove_at: out of bounds");                               \
        memmove(&(self)->data[(i)], &(self)->data[(i) + (n)],                  \
                ((self)->length - (i) - (n)) * sizeof(*(self)->data));         \
        (self)->length -= (n);                                                 \
    } while(0)

#define vect_at(slice, n)                                                      \
    (assert((usize)(n) < (slice)->length && "vect_at: index out of bounds"),   \
     &(slice)->data[(n)])

#define vect_sort(self, cb)                                                    \
    do {                                                                       \
        if((self)->data) {                                                     \
            qsort((void*)(self)->data, (self)->length, sizeof(*(self)->data),  \
                  (VectCompare)(cb));                                          \
        }                                                                      \
    } while(0)

#define vect_bsearch(key, self, cb)                                            \
    ((self)->data ? bsearch((void*)(key), (void*)(self)->data, (self)->length, \
                            sizeof(*(self)->data), (VectCompare)(cb))          \
                  : NULL);

#define vect_stable_part(self, pred)                                           \
    _vect_stable_part((void*)(self)->data, (self)->length,                     \
                      sizeof(*(self)->data), (VectPredicate)(pred));

#define vect_lower_bound(key, self, cb)                                        \
    _vect_lower_bound((const void*)(key), (void*)(self)->data, (self)->length, \
                      sizeof(*(self)->data), (VectCompare)(cb))

#define vect_upper_bound(key, self, cb)                                        \
    _vect_upper_bound((const void*)(key), (void*)(self)->data, (self)->length, \
                      sizeof(*(self)->data), (VectCompare)(cb))

#define vect_front(self)                                                       \
    (assert((self)->length && "vect_front: container is empty"),               \
     &(self)->data[0])

#define vect_back(self)                                                        \
    (assert((self)->length && "vect_back: container is empty"),                \
     &(self)->data[(self)->length - 1])

#define vect_pop_back(self)                                                    \
    (assert((self)->length && "vect_pop_back: container is empty"),            \
     (self)->data[--(self)->length])

#define vect_reserve(self, n)                                                  \
    _vect_reserve((void**)(&(self)->data), &(self)->capacity, n,               \
                  sizeof(*(self)->data))

#define vect_to_slice(SliceType, self)                                         \
    ((SliceType){.data = (self)->data, .length = (self)->length})

#define list_push(self, item)                                                  \
    do {                                                                       \
        (item)->next = (self);                                                 \
        (item)->prev = NULL;                                                   \
        if(self) (self)->prev = (item);                                        \
        (self) = (item);                                                       \
    } while(0)

#define list_insert_before(self, at, item)                                     \
    do {                                                                       \
        assert((at) && "list_insert_before: reference node is NULL");          \
        (item)->next = (at);                                                   \
        (item)->prev = (at)->prev;                                             \
        if((at)->prev)                                                         \
            (at)->prev->next = (item);                                         \
        else                                                                   \
            (self) = (item);                                                   \
        (at)->prev = (item);                                                   \
    } while(0)

#define list_insert_after(self, at, item)                                      \
    do {                                                                       \
        assert((at) && "list_insert_after: reference node is NULL");           \
        (item)->prev = (at);                                                   \
        (item)->next = (at)->next;                                             \
        if((at)->next) (at)->next->prev = (item);                              \
        (at)->next = (item);                                                   \
    } while(0)

#define list_each(it, self) for((it) = (self); (it); (it) = (it)->next)

#define list_destroy(it, self)                                                 \
    do {                                                                       \
        while(self) {                                                          \
            (it) = (self)->next;                                               \
            free(self);                                                        \
            (self) = it;                                                       \
        }                                                                      \
    } while(0)

#define stack_init(s)                                                          \
    do {                                                                       \
        (s)->top = -1;                                                         \
    } while(0)

#define stack_clear(s) stack_init(s)
#define stack_is_empty(self) ((self)->top == -1)
#define stack_capacity(s) (sizeof((s)->data) / sizeof(*(s)->data))

#define stack_push(s, ...)                                                     \
    do {                                                                       \
        assert(((s)->top < (int)stack_capacity(s) - 1) &&                      \
               "stack_push: stack overflow");                                  \
        (s)->data[++(s)->top] = __VA_ARGS__;                                   \
    } while(0)

#define stack_pop(s)                                                           \
    (assert((s)->top >= 0 && "stack_pop: stack underflow"),                    \
     (s)->data[(s)->top--])

#define stack_top(s)                                                           \
    (assert((s)->top >= 0 && "stack_top: stack is empty"), (s)->data[(s)->top])

#define str_append(self, cstr)                                                 \
    _str_append(&(self)->data, &(self)->capacity, &(self)->length, (cstr))

#define str_push(self, c)                                                      \
    _str_push(&(self)->data, &(self)->capacity, &(self)->length, (c))

#define str_clear(self)                                                        \
    do {                                                                       \
        (self)->length = 0;                                                    \
        if((self)->data) (self)->data[0] = '\0';                               \
    } while(0)

#define queue_length(self) ((self)->length)
#define queue_is_empty(self) ((self)->length == 0)

#define queue_peek_front(self)                                                 \
    (assert(!queue_is_empty(self)), (self)->data[(self)->head])

#define queue_peek_back(self)                                                  \
    (assert(!queue_is_empty(self)),                                            \
     (self)->data[(self)->head + (self)->length - 1])

#define queue_push(self, ...)                                                  \
    do {                                                                       \
        _queue_grow((void**)&(self)->data, &(self)->capacity, &(self)->head,   \
                    (self)->length, sizeof(*(self)->data));                    \
        (self)->data[(self)->head + (self)->length] = __VA_ARGS__;             \
        (self)->length++;                                                      \
    } while(0)

#define queue_pop(self, out)                                                   \
    do {                                                                       \
        assert(!queue_is_empty(self));                                         \
        *(out) = (self)->data[(self)->head];                                   \
        (self)->head++;                                                        \
        (self)->length--;                                                      \
    } while(0)

#define queue_reserve(self, n)                                                 \
    _queue_reserve((void**)&(self)->data, &(self)->capacity, (n),              \
                   sizeof(*(self)->data))

#define queue_destroy(self)                                                    \
    do {                                                                       \
        free((self)->data);                                                    \
        (self)->data = NULL;                                                   \
        (self)->length = 0;                                                    \
        (self)->capacity = 0;                                                  \
        (self)->head = 0;                                                      \
    } while(0)

#define optional_set(self, ...)                                                \
    do {                                                                       \
        (self)->value = __VA_ARGS__;                                           \
        (self)->has_value = true;                                              \
    } while(0)

#define optional_take(self)                                                    \
    (assert((self)->has_value), (self)->has_value = false, (self)->value)

#define optional_unset(self) (self)->has_value = false

#define mem_swap(T, a, b)                                                      \
    do {                                                                       \
        T _swap_tmp = *(a);                                                    \
        *(a) = *(b);                                                           \
        *(b) = _swap_tmp;                                                      \
    } while(0)

void _vect_grow(void** data, size_t* cap, size_t len, size_t itemsz);
void _vect_reserve(void** data, size_t* currcap, size_t newcap, size_t itemsz);
void _queue_grow(void** data, size_t* cap, size_t* head, size_t len,
                 size_t itemsz);
void _queue_reserve(void** data, size_t* currcap, size_t newcap, size_t itemsz);
void _str_append(char** data, size_t* cap, size_t* len, const char* cstr);
void _str_push(char** data, size_t* cap, size_t* len, char c);
size_t _vect_stable_part(void* data, size_t len, size_t itemsz,
                         VectPredicate pred);
size_t _vect_lower_bound(const void* key, void* data, size_t len, size_t itemsz,
                         VectCompare cb);
size_t _vect_upper_bound(const void* key, void* data, size_t len, size_t itemsz,
                         VectCompare cb);
