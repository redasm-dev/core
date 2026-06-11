#pragma once

#include <redasm/common.h>
#include <redasm/config.h>

typedef enum {
    RD_TYPE_NONE = 0,
    RD_TYPE_PTR,
    RD_TYPE_CPTR,
} RDTypeModifier;

typedef struct RDType {
    const char* name;
    usize count;
    RDTypeModifier mod;
} RDType;

RD_API const char* rd_integral_from_size(unsigned int size);

RD_API bool rd_get_type(RDContext* ctx, RDAddress address, RDType* t);

RD_API bool rd_auto_type(RDContext* ctx, RDAddress address, const char* name,
                         usize n, RDTypeModifier flags);

RD_API bool rd_library_type(RDContext* ctx, RDAddress address, const char* name,
                            usize n, RDTypeModifier flags);

RD_API bool rd_user_type(RDContext* ctx, RDAddress address, const char* name,
                         usize n, RDTypeModifier flags);

RD_API bool rd_type_equals(const RDType* self, const RDType* t);

static inline bool rd_type_is_ptr(const RDType* self) {
    return self->mod == RD_TYPE_PTR || self->mod == RD_TYPE_CPTR;
}

static inline bool rd_type_is_void(const RDType* t) {
    return !t->name || !(*t->name);
}
