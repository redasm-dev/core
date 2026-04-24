#pragma once

#include <redasm/common.h>
#include <redasm/config.h>
#include <redasm/plugins/processor/instruction.h>
#include <redasm/theme.h>

typedef struct RDRenderer RDRenderer;

typedef enum {
    RD_RENDERER_DEFAULT = 0U,
    RD_RENDERER_NOINDENT = 1U << 0,
    RD_RENDERER_NOADDRESS = 1U << 1,
    RD_RENDERER_NOSEGMENT = 1U << 2,
    RD_RENDERER_NOFUNCTION = 1U << 3,
    RD_RENDERER_NOREFS = 1U << 4,
    RD_RENDERER_NOCOMMENTS = 1U << 5,
    RD_RENDERER_NOCURSOR = 1U << 6,
    RD_RENDERER_NOCURSORLINE = 1U << 7,
    RD_RENDERER_NOSELECTION = 1U << 8,
    RD_RENDERER_NOHIGHLIGHT = 1U << 9,
    RD_RENDERER_RDIL = 1U << 10,
} RDRendererFlags;

typedef enum {
    RD_NUM_DEFAULT = 0,
    RD_NUM_SIGNED = 1U << 0,
    RD_NUM_PREFIX = 1U << 1,
} RDNumberFlags;

#define RD_RENDERER_GRAPH                                                      \
    (RD_RENDERER_NOADDRESS | RD_RENDERER_NOINDENT | RD_RENDERER_NOSEGMENT |    \
     RD_RENDERER_NOFUNCTION | RD_RENDERER_NOCOMMENTS)

#define RD_RENDERER_POPUP                                                      \
    (RD_RENDERER_NOADDRESS | RD_RENDERER_NOCURSOR | RD_RENDERER_NOSELECTION |  \
     RD_RENDERER_NOCURSORLINE)

#define RD_RENDERER_TEXT (~0U & ~RD_RENDERER_RDIL)

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
RD_API void rd_renderer_cnst(RDRenderer* self, i64 c, unsigned int base,
                             usize fill, RDNumberFlags flags);
RD_API void rd_renderer_mnem(RDRenderer* self, const RDInstruction* instr,
                             RDThemeKind fg);
RD_API void rd_renderer_reg(RDRenderer* self, int reg);
RD_API void rd_renderer_nop(RDRenderer* self, const char* s);
