#pragma once

#include <redasm/kb.h>

bool rd_i_kb_validate_manifest(const RDDatum* obj);
bool rd_i_kb_validate_function(const RDDatum* obj);
bool rd_i_kb_validate_compound(const RDDatum* obj);
bool rd_i_kb_validate_enum(const RDDatum* obj);
bool rd_i_kb_validate_symbol(const RDDatum* obj);
bool rd_i_kb_validate_callconv(const RDDatum* obj);
