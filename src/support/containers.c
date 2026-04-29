#include "containers.h"
#include <string.h>

#define RD_VECTOR_CAPACITY 8
#define RD_QUEUE_CAPACITY 4096

void _vect_grow(void** data, size_t* cap, size_t len, size_t itemsz) {
    if(len < *cap) return;
    *cap = *cap ? *cap * 2 : RD_VECTOR_CAPACITY;
    *data = realloc(*data, itemsz * *cap);
}

void _vect_reserve(void** data, size_t* currcap, size_t newcap, size_t itemsz) {
    if(*currcap >= newcap) return;
    *currcap = newcap;
    *data = realloc(*data, itemsz * newcap);
}

void _queue_grow(void** data, size_t* cap, size_t* head, size_t len,
                 size_t itemsz) {
    if(*head + len < *cap) return;

    char* d = (char*)*data;

    // compact first: slide live elements back to index 0
    if(*head > 0) {
        memmove(d, d + (*head * itemsz), len * itemsz);
        *head = 0;
    }

    // grow only if compaction wasn't enough
    if(len >= *cap) {
        *cap = *cap ? *cap * 2 : RD_QUEUE_CAPACITY;
        *data = realloc(*data, itemsz * *cap);
    }
}

void _queue_reserve(void** data, size_t* currcap, size_t newcap,
                    size_t itemsz) {
    if(*currcap >= newcap) return;
    *currcap = newcap;
    *data = realloc(*data, itemsz * newcap);
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

size_t _vect_stable_part(void* data, size_t len, size_t itemsz,
                         VectPredicate pred) {
    if(!len) return 0;

    char* tmp_data = (char*)malloc(len * itemsz);
    char* in_data = (char*)data;
    size_t i = 0;

    // Copy matching elements first
    for(size_t j = 0; j < len; ++j) {
        char* elem = in_data + (j * itemsz);
        if(pred(elem)) {
            memcpy(tmp_data + (i * itemsz), elem, itemsz);
            i++;
        }
    }

    size_t split = i;

    // Then copy non-matching
    for(size_t j = 0; j < len; ++j) {
        void* elem = in_data + (j * itemsz);
        if(!pred(elem)) {
            memcpy(tmp_data + (i * itemsz), elem, itemsz);
            i++;
        }
    }

    // Copy back to original data
    memcpy(in_data, tmp_data, len * itemsz);
    free(tmp_data);
    return split;
}

size_t _vect_lower_bound(const void* key, void* data, size_t len, size_t itemsz,
                         VectCompare cb) {
    size_t lo = 0, hi = len;

    while(lo < hi) {
        size_t mid = lo + ((hi - lo) / 2);
        void* elem = (char*)data + (mid * itemsz);
        if(cb(key, elem) > 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo; // == len if all elements are less than key
}

size_t _vect_upper_bound(const void* key, void* data, size_t len, size_t itemsz,
                         VectCompare cb) {
    size_t lo = 0, hi = len;

    while(lo < hi) {
        size_t mid = lo + ((hi - lo) / 2);
        void* elem = (char*)data + (mid * itemsz);
        if(cb(key, elem) >= 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo; // == len if all elements are <= key
}
