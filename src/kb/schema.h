#pragma once

#include <redasm/kb.h>

bool rd_i_kb_validate_manifest(const RDKBObject* obj);
bool rd_i_kb_validate_function(const RDKBObject* obj);
bool rd_i_kb_validate_compound(const RDKBObject* obj);
bool rd_i_kb_validate_enum(const RDKBObject* obj);
bool rd_i_kb_validate_symbol(const RDKBObject* obj);
bool rd_i_kb_validate_callconv(const RDKBObject* obj);
