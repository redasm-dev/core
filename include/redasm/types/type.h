#pragma once

#include <redasm/types/common.h>

#define RD_MAX_DEPTH SIZE_MAX

typedef struct RDResolveResult {
    RDParam field;
    usize depth;

    struct {
        usize value;
        bool has_value;
    } item_idx;
} RDResolveResult;

RD_API bool rd_type_init(RDType* self, const char* name, usize n,
                         RDTypeModifier mod, RDContext* ctx);

RD_API void rd_type_init_void(RDType* self);

RD_API usize rd_type_size(const RDType* self, const RDContext* ctx);
RD_API bool rd_type_is_string(const RDType* t);
RD_API bool rd_type_equals(const RDType* self, const RDType* t);
RD_API bool rd_type_resolve_offset(RDContext* ctx, const RDType* type,
                                   usize offset, usize min_depth,
                                   RDResolveResult* out_r);

static inline bool rd_type_is_ptr(const RDType* self) {
    return self->mod == RD_TYPE_PTR || self->mod == RD_TYPE_CPTR;
}

static inline bool rd_type_is_void(const RDType* t) { return !t->def; }

RD_API const RDTypeDef* rd_integral_typedef_from_size(unsigned int size,
                                                      const RDContext* ctx);
RD_API const char* rd_integral_from_size(unsigned int size);
RD_API const char* rd_type_to_str(const RDType* self, RDContext* ctx);

RD_API bool rd_get_type(RDContext* ctx, RDAddress address, RDType* t);

RD_API bool rd_auto_type(RDContext* ctx, RDAddress address, const char* name,
                         usize n, RDTypeModifier flags);

RD_API bool rd_library_type(RDContext* ctx, RDAddress address, const char* name,
                            usize n, RDTypeModifier flags);

RD_API bool rd_user_type(RDContext* ctx, RDAddress address, const char* name,
                         usize n, RDTypeModifier flags);
