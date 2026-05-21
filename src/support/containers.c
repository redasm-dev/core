#include "containers.h"
#include <stdint.h>
#include <string.h>

#define RD_VECTOR_CAPACITY 8

#define RD_QUEUE_CAPACITY 4096

#define HMAP_FREE 0
#define HMAP_DELETED (-1)

#define HMAP_INIT_CAPACITY 16
#define HMAP_LOAD_FACTOR_NUM 3
#define HMAP_LOAD_FACTOR_DEN 4

void _vect_grow(void** data, size_t* cap, size_t len, size_t elem_size) {
    if(len < *cap) return;
    *cap = *cap ? *cap * 2 : RD_VECTOR_CAPACITY;
    *data = realloc(*data, elem_size * *cap);
}

void _vect_reserve(void** data, size_t* currcap, size_t newcap,
                   size_t elem_size) {
    if(*currcap >= newcap) return;
    *currcap = newcap;
    *data = realloc(*data, elem_size * newcap);
}

void _queue_grow(void** data, size_t* cap, size_t* head, size_t len,
                 size_t elem_size) {
    if(*head + len < *cap) return;

    char* d = (char*)*data;

    // compact first: slide live elements back to index 0
    if(*head > 0) {
        assert(*head + len == *cap);
        memmove(d, d + (*head * elem_size), len * elem_size);
        *head = 0;
    }

    // grow only if compaction wasn't enough
    if(len >= *cap) {
        *cap = *cap ? *cap * 2 : RD_QUEUE_CAPACITY;
        *data = realloc(*data, elem_size * *cap);
    }
}

void _queue_reserve(void** data, size_t* currcap, size_t newcap,
                    size_t elem_size) {
    if(*currcap >= newcap) return;
    *currcap = newcap;
    *data = realloc(*data, elem_size * newcap);
}

void _str_append(char** data, size_t* cap, size_t* len, const char* cstr) {
    size_t slen = strlen(cstr);
    size_t newlen = *len + slen;

    if(newlen + 1 > *cap) {
        size_t newcap = *cap ? *cap * 2 : RD_VECTOR_CAPACITY;
        if(newcap < newlen + 1) newcap = newlen + 1;
        *data = realloc(*data, newcap);
        *cap = newcap;
    }

    memcpy(*data + *len, cstr, slen + 1);
    *len = newlen;
}

void _str_push(char** data, size_t* cap, size_t* len, char c) {
    if(*len + 2 > *cap) {
        size_t newcap = *cap ? *cap * 2 : RD_VECTOR_CAPACITY;
        *data = realloc(*data, newcap);
        *cap = newcap;
    }

    (*data)[(*len)++] = c;
    (*data)[*len] = '\0';
}

void _vect_ins(void** data, size_t* capacity, size_t length, size_t idx,
               size_t elem_size) {
    _vect_grow(data, capacity, length, elem_size);

    if(idx < length) {
        memmove((char*)*data + ((idx + 1) * elem_size),
                (char*)*data + (idx * elem_size), (length - idx) * elem_size);
    }
}

void _vect_del_if(const void* key, void* data, size_t* len, size_t elem_size,
                  VectDelIf cb) {
    char* p = (char*)data;
    size_t write = 0;

    for(size_t i = 0; i < *len; i++) {
        char* elem = p + (i * elem_size);

        if(!cb(key, elem)) { // no compact, preserve
            if(write != i) memcpy(p + (write * elem_size), elem, elem_size);
            write++;
        }
    }

    *len = write;
}

size_t _vect_stable_part(void* data, size_t len, size_t elem_size,
                         VectPredicate pred) {
    if(!len) return 0;

    char* tmp_data = (char*)malloc(len * elem_size);
    char* in_data = (char*)data;
    size_t i = 0;

    // Copy matching elements first
    for(size_t j = 0; j < len; ++j) {
        char* elem = in_data + (j * elem_size);
        if(pred(elem)) {
            memcpy(tmp_data + (i * elem_size), elem, elem_size);
            i++;
        }
    }

    size_t split = i;

    // Then copy non-matching
    for(size_t j = 0; j < len; ++j) {
        void* elem = in_data + (j * elem_size);
        if(!pred(elem)) {
            memcpy(tmp_data + (i * elem_size), elem, elem_size);
            i++;
        }
    }

    // Copy back to original data
    memcpy(in_data, tmp_data, len * elem_size);
    free(tmp_data);
    return split;
}

size_t _vect_lower_bound(const void* key, void* data, size_t len,
                         size_t elem_size, VectKeyCompare cb) {
    size_t lo = 0, hi = len;

    while(lo < hi) {
        size_t mid = lo + ((hi - lo) / 2);
        void* elem = (char*)data + (mid * elem_size);
        if(cb(key, elem) > 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo; // == len if all elements are less than key
}

size_t _vect_upper_bound(const void* key, void* data, size_t len,
                         size_t elem_size, VectKeyCompare cb) {
    size_t lo = 0, hi = len;

    while(lo < hi) {
        size_t mid = lo + ((hi - lo) / 2);
        void* elem = (char*)data + (mid * elem_size);
        if(cb(key, elem) >= 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo; // == len if all elements are <= key
}

size_t _vect_bsearch(const void* key, const void* data, size_t length,
                     size_t elem_size, VectKeyCompare cb) {
    if(!data) return length;

    const void* p = bsearch(key, data, length, elem_size, cb);
    if(!p) return length;

    return (size_t)((const char*)p - (const char*)data) / elem_size;
}

size_t _vect_idx(const void* p, const void* data, size_t length,
                 size_t elem_size) {
    if(!p || !data) return length;

    const char* q = (const char*)p;
    const char* b = (const char*)data;
    const char* e = b + (length * elem_size);
    if(q < b || q >= e) return length;

    // check if in the middle
    if((size_t)(q - b) % elem_size != 0) return length;

    return (size_t)(q - b) / elem_size;
}

void _hmap_rehash(void** data, size_t* capacity, size_t newcapacity,
                  size_t length, size_t elem_size) {
    size_t new_cap = *capacity ? *capacity * 2 : HMAP_INIT_CAPACITY;
    while(new_cap < newcapacity)
        new_cap *= 2;

    void* new_data =
        calloc(new_cap, elem_size); // HMAP_FREE = 0, calloc handles it
    assert(new_data);

    char* old = (char*)*data;
    size_t reinserted = 0; // try to leave when length is reached

    for(size_t i = 0; i < *capacity && reinserted < length; i++) {
        char* entry = old + (i * elem_size);
        HMapHeader* hdr = (HMapHeader*)entry;
        if(hdr->state != HMAP_OCCUPIED) continue;

        size_t slot = hdr->hash % new_cap;

        while(((HMapHeader*)((char*)new_data + (slot * elem_size)))->state ==
              HMAP_OCCUPIED)
            slot = (slot + 1) % new_cap;

        memcpy((char*)new_data + (slot * elem_size), entry, elem_size);
        reinserted++;
    }

    free(*data);
    *data = new_data;
    *capacity = new_cap;
}

void* _hmap_get(void* data, size_t capacity, const void* entry,
                size_t elem_size, HMapHash hash_fn, HMapEqual eq_fn) {
    if(!capacity) return NULL;

    size_t h = hash_fn(entry);
    size_t slot = h % capacity;

    for(size_t i = 0; i < capacity; i++) {
        char* e = (char*)data + (slot * elem_size);
        HMapHeader* hdr = (HMapHeader*)e;

        if(hdr->state == HMAP_FREE) return NULL;

        if(hdr->state == HMAP_OCCUPIED && hdr->hash == h && eq_fn(e, entry))
            return e;

        slot = (slot + 1) % capacity;
    }

    return NULL;
}

void _hmap_set(void** data, size_t* capacity, size_t* length, const void* entry,
               size_t elem_size, HMapHash hash_fn, HMapEqual eq_fn) {
    // rehash if needed
    if(*length >= (*capacity) * HMAP_LOAD_FACTOR_NUM / HMAP_LOAD_FACTOR_DEN)
        _hmap_rehash(data, capacity, *capacity * 2, *length, elem_size);

    size_t h = hash_fn(entry);
    size_t slot = h % *capacity;
    size_t deleted_slot = SIZE_MAX; // first deleted slot found

    for(size_t i = 0; i < *capacity; i++) {
        char* e = (char*)*data + (slot * elem_size);
        HMapHeader* hdr = (HMapHeader*)e;

        if(hdr->state == HMAP_FREE) {
            // key not found: insert at deleted slot if seen, else here
            size_t target = (deleted_slot != SIZE_MAX) ? deleted_slot : slot;
            char* t = (char*)*data + (target * elem_size);
            memcpy(t, entry, elem_size);
            ((HMapHeader*)t)->hash = h;
            ((HMapHeader*)t)->state = HMAP_OCCUPIED;
            (*length)++;
            return;
        }

        if(hdr->state == HMAP_DELETED && deleted_slot == SIZE_MAX)
            deleted_slot = slot; // remember first deleted slot

        if(hdr->state == HMAP_OCCUPIED && hdr->hash == h && eq_fn(e, entry)) {
            // key exists: update in place
            memcpy(e, entry, elem_size);
            hdr->hash = h;
            hdr->state = HMAP_OCCUPIED;
            return; // length unchanged
        }

        slot = (slot + 1) % *capacity;
    }

    if(deleted_slot != SIZE_MAX) {
        // table full of tombstones: insert at first deleted slot
        char* t = (char*)*data + (deleted_slot * elem_size);
        memcpy(t, entry, elem_size);
        ((HMapHeader*)t)->hash = h;
        ((HMapHeader*)t)->state = HMAP_OCCUPIED;
        (*length)++;
    }
}

void _hmap_del(void* data, size_t capacity, size_t* length, const void* entry,
               size_t elem_size, HMapHash hash_fn, HMapEqual eq_fn) {
    if(!capacity) return;

    size_t h = hash_fn(entry);
    size_t slot = h % capacity;

    for(size_t i = 0; i < capacity; i++) {
        char* e = (char*)data + (slot * elem_size);
        HMapHeader* hdr = (HMapHeader*)e;

        if(hdr->state == HMAP_FREE) return; // not found

        if(hdr->state == HMAP_OCCUPIED && hdr->hash == h && eq_fn(e, entry)) {
            hdr->state = HMAP_DELETED;
            (*length)--;
            return;
        }

        slot = (slot + 1) % capacity;
    }
}
