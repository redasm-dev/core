#pragma once

#include <redasm/config.h>

typedef struct RDKBObject RDKBObject;

RD_API RDKBObject* rd_kb_load(const char* name);

RD_API usize rd_kbobject_get_length(const RDKBObject* self);
RD_API const char* rd_kbobject_get_str(RDKBObject* self, const char* key);
