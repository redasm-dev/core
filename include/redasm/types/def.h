#pragma once

#include <redasm/config.h>
#include <redasm/types/type.h>

typedef struct RDTypeDef RDTypeDef;

// clang-format off
RD_API RDTypeDef* rd_typedef_create_func(const char* name, RDContext* ctx);
RD_API RDTypeDef* rd_typedef_create_struct(const char* name, RDContext* ctx);
RD_API RDTypeDef* rd_typedef_create_union(const char* name, RDContext* ctx);
RD_API RDTypeDef* rd_typedef_create_enum(const char* name, const char* type, RDContext* ctx);
RD_API bool rd_typedef_add_member(RDTypeDef* self, const char* type, const char* name, usize n, usize flags, RDContext* ctx);
RD_API bool rd_typedef_add_enumval(RDTypeDef* self, const char* name, i64 value, RDContext* ctx);
RD_API bool rd_typedef_add_arg(RDTypeDef* self, const char* type, const char* name, usize n, usize flags, RDContext* ctx);
RD_API bool rd_typedef_set_ret(RDTypeDef* self, const char* type, usize n, usize flags, RDContext* ctx);
RD_API bool rd_typedef_register(RDTypeDef* self, RDContext* ctx);
// clang-format on
