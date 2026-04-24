#include "stringpool.h"
#include "support/containers.h"
#include <redasm/support/utils.h>
#include <string.h>

const char* rd_i_strpool_intern(RDStringPool* self, const char* s) {
    if(!s) return NULL;

    for(usize i = 0; i < self->length; i++)
        if(!strcmp(self->data[i], s)) return self->data[i];

    char* dup = rd_strdup(s);
    vect_push(self, dup);
    return dup;
}

void rd_i_strpool_destroy(RDStringPool* self) {
    char** s;
    vect_each(s, self) rd_free(*s);
    vect_destroy(self);
}
