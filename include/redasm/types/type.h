#pragma once

#include <redasm/common.h>
#include <redasm/config.h>

typedef enum {
    RD_TYPE_NONE = 0,
    RD_TYPE_ISPOINTER = (1 << 0),
} RDTypeFlags;

typedef struct RDType {
    const char* name;
    usize count;
    RDTypeFlags flags;
} RDType;

// rd_size_of: returns the size of a registered type in bytes, without padding.
// Use this instead of sizeof() for struct types to avoid platform-specific
// padding issues when parsing binary formats.
// For primitive types (u8, u16, u32, u64), sizeof() is always safe.
RD_API usize rd_size_of(const RDContext* ctx, const char* name, usize n);

RD_API const char* rd_integral_from_size(unsigned int size);

RD_API bool rd_get_type(RDContext* ctx, RDAddress address, RDType* t);

RD_API bool rd_auto_type(RDContext* ctx, RDAddress address, const char* name,
                         usize n, RDTypeFlags flags);

RD_API bool rd_library_type(RDContext* ctx, RDAddress address, const char* name,
                            usize n, RDTypeFlags flags);

RD_API bool rd_user_type(RDContext* ctx, RDAddress address, const char* name,
                         usize n, RDTypeFlags flags);
