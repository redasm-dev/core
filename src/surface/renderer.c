#include "renderer.h"
#include "core/context.h"
#include "core/engine.h"
#include "rdil/rdil.h"
#include "support/containers.h"
#include "support/utils.h"
#include <ctype.h>

#define RD_SURFACE_BUF_INITIAL_SIZE 1024

static void _rd_cells_char(RDCellVect* self, char ch, RDThemeKind fg,
                           RDThemeKind bg) {
    if(fg == RD_THEME_DEFAULT) fg = RD_THEME_FOREGROUND;
    if(bg == RD_THEME_DEFAULT) bg = RD_THEME_BACKGROUND;
    vect_push(self, (RDCell){.ch = ch, .fg = fg, .bg = bg});
}

static void _rd_rows_destroy(RDRowVect* rv) {
    RDCellVect* cells;
    vect_each(cells, rv) vect_destroy(cells);
}

static void _rd_renderer_num(RDRenderer* self, i64 c, unsigned int base,
                             usize fill, RDNumberFlags flags, RDThemeKind fg) {
    RDBaseParams p = {
        .base = base ? base : 16,
        .with_prefix = flags & RD_NUM_PREFIX,
        .with_sign = flags & RD_NUM_SIGNED,
        .fill = fill,
    };

    rd_renderer_text(self, rd_i_to_base(c, &p), fg, RD_THEME_BACKGROUND);
}

static bool _rd_is_char_skippable(char ch) {
    if(ch == '_' || ch == '@' || ch == '.') return false;
    return isspace((int)ch) || ispunct((int)ch);
}

static const char* _rd_renderer_word_at(RDRenderer* self, const RDRowVect* rows,
                                        int row, int col) {
    if(row >= (int)vect_length(rows)) return NULL;

    const RDCellVect* cells = vect_at(rows, row);
    if(col >= (int)vect_length(cells)) col = vect_length(cells) - 1;

    if(_rd_is_char_skippable(vect_at(cells, col)->ch)) return NULL;

    vect_clear(&self->word_buf);

    for(int i = col; i-- > 0;) {
        if(_rd_is_char_skippable(vect_at(cells, i)->ch)) break;
        col--;
    }

    for(int i = col; i < (int)vect_length(cells); i++) {
        if(_rd_is_char_skippable(vect_at(cells, i)->ch)) break;
        vect_push(&self->word_buf, vect_at(cells, i)->ch);
    }

    vect_push(&self->word_buf, 0);
    return *self->word_buf.data ? self->word_buf.data : NULL;
}

static void _rd_renderer_calc_auto_column(RDRenderer* self) {
    if(self->columns || vect_is_empty(&self->rows_back)) return;

    self->auto_columns =
        rd_i_max(self->auto_columns, vect_length(vect_back(&self->rows_back)));
}

RDRenderer* rd_i_renderer_create(RDContext* ctx, RDRenderFlags flags) {
    RDRenderer* self = malloc(sizeof(*self));

    *self = (RDRenderer){
        .context = ctx,
        .flags = flags,
        .mode = RD_RM_NORMAL,
    };

    vect_reserve(&self->text_buf, RD_SURFACE_BUF_INITIAL_SIZE);
    vect_reserve(&self->comment_buf, RD_SURFACE_BUF_INITIAL_SIZE);
    return self;
}

void rd_i_renderer_destroy(RDRenderer* self) {
    vect_destroy(&self->xrefs);
    vect_destroy(&self->comment_buf);
    vect_destroy(&self->text_buf);
    vect_destroy(&self->word_buf);
    _rd_rows_destroy(&self->rows_back);
    _rd_rows_destroy(&self->rows_front);
    vect_destroy(&self->rows_back);
    vect_destroy(&self->rows_front);
    free(self->hl_word);
    free(self);
}

void rd_i_renderer_clear(RDRenderer* self) {
    _rd_rows_destroy(&self->rows_back);
    vect_clear(&self->rows_back);
    self->auto_columns = 0;
}

void rd_i_renderer_swap(RDRenderer* self) {
    mem_swap(RDRowVect, &self->rows_back, &self->rows_front);
}

void rd_i_renderer_set_mode(RDRenderer* self, RDRenderMode m) {
    self->mode = m;
}

void rd_i_renderer_set_cursor_visible(RDRenderer* self, bool b) {
    if(b)
        self->flags &= ~RD_RF_NO_CURSOR;
    else
        self->flags |= RD_RF_NO_CURSOR;
}

void rd_i_renderer_fill_columns(RDRenderer* self) {
    _rd_renderer_calc_auto_column(self);
    usize ncols = self->columns > 0 ? self->columns : self->auto_columns;

    RDCellVect* cells;
    vect_each(cells, &self->rows_back) {
        cells->content_length = vect_length(cells);

        while(cells->length <= ncols)
            _rd_cells_char(cells, ' ', RD_THEME_FOREGROUND,
                           RD_THEME_BACKGROUND);
    }
}

void rd_i_renderer_highlight_row(RDRenderer* self, int row) {
    if(rd_i_renderer_has_flag(self, RD_RF_NO_CURSOR_LINE) ||
       (row >= (int)self->rows_back.length))
        return;

    RDCellVect* cells = vect_at(&self->rows_back, row);

    RDCell* c;
    vect_each(c, cells) {
        if(c->bg == RD_THEME_BACKGROUND) c->bg = RD_THEME_SEEK;
    }
}

void rd_i_renderer_highlight_cursor(RDRenderer* self, int row, int col) {
    if(rd_i_renderer_has_flag(self, RD_RF_NO_CURSOR) ||
       vect_is_empty(&self->rows_back))
        return;

    if(row >= (int)vect_length(&self->rows_back))
        row = (int)vect_length(&self->rows_back) - 1;

    if(vect_is_empty(vect_at(&self->rows_back, row))) return;

    RDCellVect* cells = vect_at(&self->rows_back, row);
    if(col >= (int)vect_length(cells)) col = (int)vect_length(cells) - 1;

    vect_at(cells, col)->fg = RD_THEME_CURSOR_FG;
    vect_at(cells, col)->bg = RD_THEME_CURSOR_BG;
}

void rd_i_renderer_highlight_words(RDRenderer* self, int row, int col) {
    if(rd_i_renderer_has_flag(self, RD_RF_NO_HIGHLIGHT)) return;

    const char* word = self->hl_word;
    if(!word) word = _rd_renderer_word_at(self, &self->rows_back, row, col);
    if(!word) return;

    RDCellVect* cells;
    vect_each(cells, &self->rows_back) {
        for(usize i = 0; i < vect_length(cells); i++) {
            usize endidx = i;
            bool found = true;

            for(const char* w = word; *w; w++) {
                if(endidx >= vect_length(cells)) break;

                const RDCell* c = vect_at(cells, endidx);
                endidx++;
                if(c->ch == *w) continue;

                found = false;
                endidx = i;
                break;
            }

            for(usize j = i; found && j < endidx; j++) {
                vect_at(cells, j)->fg = RD_THEME_HIGHLIGHT_FG;
                vect_at(cells, j)->bg = RD_THEME_HIGHLIGHT_BG;
            }
        }
    }
}

void rd_i_renderer_highlight_selection(RDRenderer* self, int startrow,
                                       int startcol, int endrow, int endcol) {
    if(rd_i_renderer_has_flag(self, RD_RF_NO_SELECTION)) return;

    for(int row = startrow;
        row < (int)vect_length(&self->rows_back) && row <= endrow; row++) {
        RDCellVect* r = vect_at(&self->rows_back, row);
        int sc = 0, ec = 0;

        if(!vect_is_empty(r)) ec = vect_length(r) - 1;
        if(row == startrow) sc = startcol;
        if(row == endrow) ec = endcol;

        for(int col = sc; col < (int)vect_length(r) && col < ec; col++) {
            vect_at(r, col)->fg = RD_THEME_SELECTION_FG;
            vect_at(r, col)->bg = RD_THEME_SELECTION_BG;
        }
    }
}

void rd_i_renderer_new_row(RDRenderer* self, const RDListingItem* item) {
    _rd_renderer_calc_auto_column(self);

    vect_push(&self->rows_back, (RDCellVect){
                                    .index = self->listing_idx,
                                    .address = item->address,
                                });

    if(self->columns) vect_reserve(vect_back(&self->rows_back), self->columns);

    if(!rd_i_renderer_has_flag(self, RD_RF_NO_ADDRESS)) {
        rd_renderer_norm(self, item->segment->base.name);
        rd_renderer_norm(self, ":");

        unsigned int proc_int = rd_processor_get_int_size(self->context);

        unsigned int size =
            item->segment->base.unit ? item->segment->base.unit : proc_int;

        const char* address = rd_i_to_hex(item->address, size);

        rd_renderer_norm(self, address);
        rd_renderer_ws(self, 2);
    }

    if(!rd_i_renderer_has_flag(self, RD_RF_NO_INDENT))
        rd_renderer_ws(self, item->indent);
}

void rd_renderer_text(RDRenderer* self, const char* s, RDThemeKind fg,
                      RDThemeKind bg) {
    assert(s && "invalid chunk string");

    RDCellVect* cells = vect_back(&self->rows_back);

    for(char ch = *s; ch; ch = *++s) {
        if(self->columns && vect_length(cells) >= self->columns) break;

        switch(ch) {
            case '\t':
                _rd_cells_char(cells, '\\', fg, bg);
                _rd_cells_char(cells, 't', fg, bg);
                break;

            case '\n':
                _rd_cells_char(cells, '\\', fg, bg);
                _rd_cells_char(cells, 'n', fg, bg);
                break;

            case '\r':
                _rd_cells_char(cells, '\\', fg, bg);
                _rd_cells_char(cells, 'r', fg, bg);
                break;

            case '\v':
                _rd_cells_char(cells, '\\', fg, bg);
                _rd_cells_char(cells, 'v', fg, bg);
                break;

            default: _rd_cells_char(cells, ch, fg, bg); break;
        }
    }
}

void rd_renderer_word(RDRenderer* self, const char* s, RDThemeKind fg,
                      RDThemeKind bg) {
    rd_renderer_ws(self, 1);
    rd_renderer_text(self, s, fg, bg);
    rd_renderer_ws(self, 1);
}

void rd_i_renderer_rdil(RDRenderer* self, const RDListingItem* item) {
    RDContext* ctx = self->context;
    RDInstruction instr = {0};
    rd_i_il_init(&ctx->il_buf);

    if(rd_i_processor_has_lift(ctx) &&
       rd_i_engine_decode(self->context, item->address, &instr))
        rd_i_processor_lift(ctx, &instr, &ctx->il_buf);

    // render 'unknown' if empty
    rd_i_il_render(self, &ctx->il_buf);
}

void rd_i_renderer_flags(RDRenderer* self, const RDListingItem* item) {
    static const struct {
        bool (*pred)(const RDFlagsBuffer*, usize);
        const char* name;
        RDThemeKind fg;
    } PREDS[] = {
        {rd_flagsbuffer_has_func, "FUNC", RD_THEME_FUNCTION},
        {rd_flagsbuffer_has_noret, "NORET", RD_THEME_STOP},
        {rd_flagsbuffer_has_call, "CALL", RD_THEME_CALL},
        {rd_flagsbuffer_has_jump, "JUMP", RD_THEME_JUMP},
        {rd_flagsbuffer_has_cond, "COND", RD_THEME_JUMP_COND},
        {rd_flagsbuffer_has_flow, "FLOW", RD_THEME_FOREGROUND},
        {rd_flagsbuffer_has_code, "CODE", RD_THEME_FLAG_CODE},
        {rd_flagsbuffer_has_data, "DATA", RD_THEME_FLAG_DATA},
        {rd_flagsbuffer_has_tail, "TAIL", RD_THEME_FOREGROUND},
        {rd_flagsbuffer_has_name, "NAME", RD_THEME_FOREGROUND},
        {rd_flagsbuffer_has_exported, "EXPORTED", RD_THEME_FOREGROUND},
        {rd_flagsbuffer_has_imported, "IMPORTED", RD_THEME_FOREGROUND},
    };

    const int N_PREDS = sizeof(PREDS) / sizeof(*PREDS);
    usize idx = rd_i_address2index(item->segment, item->address);

    bool first = true;
    for(int i = 0; i < N_PREDS; i++) {
        if(!PREDS[i].pred(item->segment->flags, idx)) continue;
        if(!first) rd_renderer_norm(self, " | ");
        rd_renderer_text(self, PREDS[i].name, PREDS[i].fg, RD_THEME_BACKGROUND);
        first = false;
    }

    if(first) rd_renderer_unkn(self);
}

void rd_i_renderer_instr(RDRenderer* self, const RDListingItem* item) {
    RDInstruction instr = {0};

    if(rd_i_engine_decode(self->context, item->address, &instr)) {
        rd_i_processor_render_mnemonic(self->context, self, &instr);
        rd_renderer_ws(self, 1);

        rd_foreach_operand(i, op, &instr) {
            if(i > 0) rd_renderer_norm(self, ", ");
            rd_i_processor_render_operand(self->context, self, &instr, i);
        }
    }
    else
        rd_renderer_unkn(self);
}

void rd_i_renderer_set_current_item(RDRenderer* self, LIndex idx,
                                    const RDListingItem* item) {
    self->curr_address = item->address;
    self->listing_idx = idx;
}

void rd_renderer_norm(RDRenderer* self, const char* s) {
    rd_renderer_text(self, s, RD_THEME_FOREGROUND, RD_THEME_BACKGROUND);
}

void rd_renderer_ws(RDRenderer* self, int n) {
    for(int i = 0; i < n; i++)
        rd_renderer_norm(self, " ");
}

void rd_renderer_str(RDRenderer* self, const char* s, bool quoted) {
    if(quoted)
        rd_renderer_text(self, "\"", RD_THEME_STRING, RD_THEME_BACKGROUND);

    rd_renderer_text(self, s, RD_THEME_STRING, RD_THEME_FOREGROUND);

    if(quoted)
        rd_renderer_text(self, "\"", RD_THEME_STRING, RD_THEME_BACKGROUND);
}

void rd_renderer_unkn(RDRenderer* self) {
    rd_renderer_text(self, "???", RD_THEME_MUTED, RD_THEME_FOREGROUND);
}

void rd_renderer_mnem(RDRenderer* self, const RDInstruction* instr,
                      RDThemeKind fg) {
    const char* mnem = rd_i_processor_get_mnemonic(self->context, instr);
    if(!mnem) mnem = rd_i_to_dec(instr->id);
    rd_renderer_text(self, mnem, fg, RD_THEME_BACKGROUND);
}

void rd_renderer_reg(RDRenderer* self, int reg) {
    const char* regname = rd_i_processor_get_register(self->context, reg);
    if(!regname) regname = rd_i_to_dec(reg);

    rd_renderer_text(self, regname, RD_THEME_REG, RD_THEME_BACKGROUND);
}

void rd_renderer_nop(RDRenderer* self, const char* s) {
    rd_renderer_text(self, s, RD_THEME_MUTED, RD_THEME_BACKGROUND);
}

void rd_renderer_loc(RDRenderer* self, RDAddress address, usize fill,
                     RDNumberFlags flags) {
    RDName n;
    bool hasname = false;

    if(!rd_i_renderer_has_flag(self, RD_RF_NO_NAMES))
        hasname = rd_i_get_name(self->context, address, true, &n);

    if(hasname)
        rd_renderer_text(self, n.value, RD_THEME_LOCATION, RD_THEME_BACKGROUND);
    else
        _rd_renderer_num(self, address, 16, fill, flags, RD_THEME_LOCATION);
}

void rd_renderer_cnst(RDRenderer* self, i64 c, unsigned int base, usize fill,
                      RDNumberFlags flags) {
    _rd_renderer_num(self, c, base, fill, flags, RD_THEME_NUMBER);
}

RDContext* rd_renderer_get_context(RDRenderer* self) { return self->context; }

const char* rd_i_renderer_get_text(RDRenderer* self, RDSurfacePos startpos,
                                   RDSurfacePos endpos) {
    usize len = vect_length(&self->rows_front);

    vect_clear(&self->text_buf);

    for(int i = startpos.row; i < (int)len && i <= endpos.row; i++) {
        if(!vect_is_empty(&self->text_buf)) vect_push(&self->text_buf, '\n');

        const RDCellVect* r = vect_at(&self->rows_front, i);
        usize s = 0, e = 0;

        if(!vect_is_empty(r)) e = vect_length(r) - 1;
        if(i == startpos.row) s = startpos.col;
        if(i == endpos.row) e = endpos.col;

        for(usize j = s; j <= e; j++)
            vect_push(&self->text_buf, r->data[j].ch);
    }

    vect_push(&self->text_buf, 0);
    return self->text_buf.data;
}

int rd_i_renderer_index_of(const RDRenderer* self, RDAddress address) {
    const RDRowVect* rows = &self->rows_front;

    for(usize i = 0; i < vect_length(rows); i++) {
        if(vect_at(rows, i)->address >= address) return (int)i;
    }

    return -1;
}

int rd_i_renderer_last_index_of(const RDRenderer* self, RDAddress address) {
    const RDRowVect* rows = &self->rows_front;
    int res = -1;

    for(usize i = 0; i < vect_length(rows); i++) {
        if(vect_at(rows, i)->address > address) break;
        res = (int)i;
    }

    return res;
}

void rd_i_renderer_set_highlight_word(RDRenderer* self, const char* w) {
    if(self->hl_word) free(self->hl_word);
    self->hl_word = w && *w ? rd_strdup(w) : NULL;
}

void rd_i_renderer_fit(const RDRenderer* self, int* row, int* col) {
    if(vect_is_empty(&self->rows_front)) {
        *row = 0;
        *col = 0;
        return;
    }

    int nrows = (int)vect_length(&self->rows_front);
    if(*row >= nrows) *row = nrows - 1;

    int ncols = (int)vect_length(vect_at(&self->rows_front, *row));
    if(!ncols)
        *col = 0;
    else if(*col >= ncols)
        *col = ncols - 1;
}

bool rd_i_renderer_is_index_visible(const RDRenderer* self, LIndex index) {
    if(vect_is_empty(&self->rows_front)) return false;

    const RDCellVect* first = vect_front(&self->rows_front);
    const RDCellVect* last = vect_back(&self->rows_front);
    return index >= first->index && index < last->index;
}

const char* rd_i_renderer_get_word_under_pos(RDRenderer* self,
                                             const RDSurfacePos* pos) {
    if(!pos) return NULL;
    return _rd_renderer_word_at(self, &self->rows_front, pos->row, pos->col);
}

bool rd_i_renderer_get_address_under_pos(RDRenderer* self,
                                         const RDSurfacePos* pos,
                                         RDAddress* address) {
    const char* word = rd_i_renderer_get_word_under_pos(self, pos);
    return rd_get_address(self->context, word, address);
}

bool rd_i_renderer_get_address(const RDRenderer* self, RDSurfacePos pos,
                               RDAddress* address) {
    const RDRowVect* rows = &self->rows_front;
    if(vect_is_empty(rows)) return false;

    usize row = (usize)pos.row;
    if(row >= vect_length(rows)) row = vect_length(rows) - 1;
    if(address) *address = vect_at(rows, row)->address;
    return true;
}

RDRowSlice rd_i_renderer_get_row(const RDRenderer* self, usize idx) {
    if(idx >= vect_length(&self->rows_front)) return (RDRowSlice){0};

    return (RDRowSlice){
        .data = vect_at(&self->rows_front, idx)->data,
        .length = vect_at(&self->rows_front, idx)->length,
        .content_length = vect_at(&self->rows_front, idx)->content_length,
    };
}

usize rd_i_renderer_get_row_count(const RDRenderer* self) {
    return vect_length(&self->rows_front);
}

bool rd_i_renderer_select_word(RDRenderer* self, int row, int col,
                               RDSurfacePos* startpos, RDSurfacePos* endpos) {
    if(row >= (int)vect_length(&self->rows_front)) return false;

    const RDCellVect* r = vect_at(&self->rows_front, row);
    if(col >= (int)vect_length(r)) col = (int)vect_length(r) - 1;

    if(_rd_is_char_skippable(vect_at(r, col)->ch)) return false;

    usize startcol = 0, endcol = 0;

    for(int i = col; i-- > 0;) {
        const RDCell* cell = vect_at(r, i);
        if(_rd_is_char_skippable(cell->ch)) {
            startcol = i + 1;
            break;
        }
    }

    for(int i = col; i < (int)vect_length(r); i++) {
        const RDCell* cell = vect_at(r, i);
        if(_rd_is_char_skippable(cell->ch)) {
            endcol = i - 1;
            break;
        }
    }

    if(startcol == endcol) return false;

    *startpos = (RDSurfacePos){.row = row, .col = startcol};
    *endpos = (RDSurfacePos){.row = row, .col = endcol};
    return true;
}

void rd_i_renderer_write_text(RDRenderer* self, RDCharVect* v) {
    usize cols = self->columns;
    vect_clear(v);

    if(cols && vect_capacity(v) < cols)
        vect_reserve(v, (cols * vect_length(&self->rows_front)) + 1);

    const RDCellVect* r;
    vect_each(r, &self->rows_front) {
        if(!vect_is_empty(v)) vect_push(v, '\n');

        const RDCell* c;
        vect_each(c, r) vect_push(v, c->ch);
    }

    vect_push(v, 0);
}
