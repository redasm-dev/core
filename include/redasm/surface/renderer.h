#pragma once

#include <redasm/common.h>
#include <redasm/config.h>
#include <redasm/plugins/processor/instruction.h>
#include <redasm/surface/common.h>
#include <redasm/theme.h>

typedef struct RDRenderer RDRenderer;

RD_API RDContext* rd_renderer_get_context(RDRenderer* self);
RD_API void rd_renderer_ws(RDRenderer* self, int n);
RD_API void rd_renderer_norm(RDRenderer* self, const char* s);
RD_API void rd_renderer_text(RDRenderer* self, const char* s, RDThemeKind fg,
                             RDThemeKind bg);
RD_API void rd_renderer_word(RDRenderer* self, const char* s, RDThemeKind fg,
                             RDThemeKind bg);
RD_API void rd_renderer_str(RDRenderer* self, const char* s, bool quoted);
RD_API void rd_renderer_unkn(RDRenderer* self);
RD_API void rd_renderer_loc(RDRenderer* self, RDAddress address, usize fill,
                            RDNumberFlags flags);
RD_API void rd_renderer_num(RDRenderer* self, i64 c, unsigned int base,
                            usize fill, RDNumberFlags flags);
RD_API void rd_renderer_mnem(RDRenderer* self, const RDInstruction* instr,
                             RDThemeKind fg);
RD_API void rd_renderer_reg(RDRenderer* self, int reg);
RD_API void rd_renderer_nop(RDRenderer* self, const char* s);
