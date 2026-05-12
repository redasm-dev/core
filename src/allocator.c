#include <redasm/allocator.h>
#include <stdio.h>
#include <stdlib.h>

void* rd_alloc(usize sz) {
    void* p = malloc(sz);

    if(!p) {
        fprintf(stderr, "FATAL: failed to allocate %zu bytes.\n", sz);
        abort();
    }

    return p;
}

void* rd_alloc0(usize n, usize sz) {
    void* p = calloc(n, sz);

    if(!p) {
        fprintf(stderr,
                "FATAL: failed to zero-allocate %zu element(s) of %zu bytes.\n",
                n, sz);
        abort();
    }

    return p;
}

void rd_free(void* p) { free(p); }
