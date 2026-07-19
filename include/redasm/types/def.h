#pragma once

#include <redasm/config.h>
#include <redasm/types/common.h>

// clang-format off
RD_API RDTypeDef* rd_typedef_create_func(const char* name, RDContext* ctx);
RD_API RDTypeDef* rd_typedef_create_struct(const char* name, RDContext* ctx);
RD_API RDTypeDef* rd_typedef_create_union(const char* name, RDContext* ctx);
RD_API RDTypeDef* rd_typedef_create_enum(const char* name, const char* type, RDContext* ctx);
RD_API RDParamSlice rd_typedef_get_members(const RDTypeDef* self);
RD_API RDParamSlice rd_typedef_get_args(const RDTypeDef* self);
RD_API RDType rd_typedef_get_ret(const RDTypeDef* self);
RD_API const RDTypeDef* rd_typedef_get_base_type(const RDTypeDef* self);
RD_API const char* rd_typedef_name(const RDTypeDef* self);
RD_API usize rd_typedef_size(const RDTypeDef* self);
RD_API void rd_typedef_destroy(RDTypeDef* self);
RD_API bool rd_typedef_resolve_offset(RDContext* ctx, const RDTypeDef* tdef, usize offset, const RDParam** m);
RD_API bool rd_typedef_add_member(RDTypeDef* self, const char* type, const char* name, usize n, RDTypeModifier mod, RDContext* ctx);
RD_API bool rd_typedef_add_enumval(RDTypeDef* self, const char* name, i64 value, RDContext* ctx);
RD_API bool rd_typedef_add_arg(RDTypeDef* self, const char* type, const char* name, usize n, RDTypeModifier mod, RDContext* ctx);
RD_API bool rd_typedef_set_ret(RDTypeDef* self, const char* type, usize n, RDTypeModifier mod, RDContext* ctx);
RD_API bool rd_typedef_set_noret(RDTypeDef* self, bool b);
RD_API bool rd_typedef_register(RDTypeDef* self, RDContext* ctx);
// clang-format on
