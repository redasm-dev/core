#include "renderer.h"
#include "core/context.h"
#include "core/engine.h"
#include "io/flagsbuffer.h"
#include "plugins/processor/processor.h"
#include "rdil/rdil.h"
#include "rdil/renderer.h"
#include "support/containers.h"
#include "support/utils.h"

#define RD_SURFACE_BUF_INITIAL_SIZE 1024

static const char* _rd_renderer_word_at(RDRenderer* self, const RDRowVect* rows,
                                        int row, int col) {
    if(row >= (int)vect_length(rows)) return NULL;

    RDRow* r = vect_at(rows, row);
    if(col >= rd_i_row_length(r)) col = rd_i_row_length(r) - 1;

    vect_clear(&self->word_buf);

    unsigned int group_idx = rd_i_row_data_at(r, col)->group_idx;
    if(!group_idx) return NULL;

    for(int i = col; i-- > 0;) {
        if(rd_i_row_data_at(r, i)->group_idx != group_idx) break;
        col--;
    }

    for(int i = col; i < rd_i_row_length(r); i++) {
        if(rd_i_row_data_at(r, i)->group_idx != group_idx) break;
        rd_i_cp_push_utf8(&self->word_buf, rd_i_row_cell_at(r, i)->cp);
    }

    vect_push(&self->word_buf, 0);
    return *self->word_buf.data ? self->word_buf.data : NULL;
}

static void _rd_renderer_calc_auto_column(RDRenderer* self) {
    if(self->columns || vect_is_empty(&self->rows_back)) return;

    int lastlen = rd_i_row_length(vect_last(&self->rows_back));
    self->auto_columns = rd_i_max_i(self->auto_columns, lastlen);
}

static void _rd_renderer_num(RDRenderer* self, i64 c, unsigned int base,
                             unsigned int fill, RDThemeKind fg,
                             RDNumberFlags flags) {
    RDBaseParams p = {
        .base = base ? base : 16,
        .with_prefix = flags & RD_NUM_PREFIX,
        .with_sign = flags & RD_NUM_SIGNED,
        .fill = fill,
    };

    rd_renderer_text(self, rd_i_to_base(c, &p), fg, RD_THEME_BACKGROUND);
}

RDRenderer* rd_i_renderer_create(RDContext* ctx, RDRenderFlags flags) {
    RDRenderer* self = rd_alloc(sizeof(*self));

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
    rd_i_rowvect_destroy(&self->rows_back);
    rd_i_rowvect_destroy(&self->rows_front);
    vect_destroy(&self->rows_back);
    vect_destroy(&self->rows_front);
    vect_destroy(&self->instr_buf);
    rd_free(self->hl_word);
    rd_free(self);
}

void rd_i_renderer_clear(RDRenderer* self) {
    rd_i_rowvect_destroy(&self->rows_back);
    vect_clear(&self->rows_back);
    self->auto_columns = 0;
    self->group_idx = 0;
}

void rd_i_renderer_swap(RDRenderer* self) {
    mem_swap(RDRowVect, &self->rows_back, &self->rows_front);
    self->group_idx = 0;
}

void rd_i_renderer_set_mode(RDRenderer* self, RDRenderMode m) {
    self->mode = m;
}

void rd_i_renderer_set_cursor_visible(RDRenderer* self, bool b) {
    if(b)
        self->flags &= (RDRenderFlags)(~RD_RF_NO_CURSOR);
    else
        self->flags |= RD_RF_NO_CURSOR;
}

void rd_i_renderer_fill_columns(RDRenderer* self) {
    _rd_renderer_calc_auto_column(self);
    int ncols = self->columns > 0 ? self->columns : self->auto_columns;

    RDRow* row;
    vect_each(row, &self->rows_back) {
        row->content_length = rd_i_row_length(row);

        // un-data fill characters
        RDCellData olddata = row->curr_data;
        row->curr_data = rd_i_default_cell_data();

        while(rd_i_row_length(row) <= ncols) {
            rd_i_row_push(row, ' ', RD_THEME_FOREGROUND, RD_THEME_BACKGROUND);
        }

        row->curr_data = olddata;
    }
}

void rd_i_renderer_highlight_row(RDRenderer* self, int r) {
    if(rd_i_renderer_has_flag(self, RD_RF_NO_CURSOR_LINE) ||
       (r >= (int)self->rows_back.length))
        return;

    RDRow* row = vect_at(&self->rows_back, r);

    RDCell* c;
    vect_each(c, &row->cells) {
        if(c->bg == RD_THEME_BACKGROUND) c->bg = RD_THEME_SEEK;
    }
}

void rd_i_renderer_highlight_cursor(RDRenderer* self, int row, int col) {
    if(rd_i_renderer_has_flag(self, RD_RF_NO_CURSOR) ||
       vect_is_empty(&self->rows_back))
        return;

    if(row >= (int)vect_length(&self->rows_back))
        row = (int)vect_length(&self->rows_back) - 1;

    if(rd_i_row_is_empty(vect_at(&self->rows_back, row))) return;

    RDRow* r = vect_at(&self->rows_back, row);
    if(col >= rd_i_row_length(r)) col = rd_i_row_length(r) - 1;

    rd_i_row_cell_at(r, col)->fg = RD_THEME_CURSOR_FG;
    rd_i_row_cell_at(r, col)->bg = RD_THEME_CURSOR_BG;
}

void rd_i_renderer_highlight_words(RDRenderer* self, int row, int col) {
    if(rd_i_renderer_has_flag(self, RD_RF_NO_HIGHLIGHT)) return;

    const char* word = self->hl_word;
    if(!word) word = _rd_renderer_word_at(self, &self->rows_back, row, col);
    if(!word) return;

    RDRow* r;
    vect_each(r, &self->rows_back) {
        for(int i = 0; i < rd_i_row_length(r); i++) {
            int endidx = i;
            bool found = true;

            for(const char* w = word; *w; w++) {
                if(endidx >= rd_i_row_length(r)) break;

                const RDCell* c = rd_i_row_cell_at(r, endidx);
                endidx++;
                if((char)c->cp == *w) continue;

                found = false;
                endidx = i;
                break;
            }

            for(int j = i; found && j < endidx; j++) {
                rd_i_row_cell_at(r, j)->fg = RD_THEME_HIGHLIGHT_FG;
                rd_i_row_cell_at(r, j)->bg = RD_THEME_HIGHLIGHT_BG;
            }
        }
    }
}

void rd_i_renderer_highlight_selection(RDRenderer* self, int startrow,
                                       int startcol, int endrow, int endcol) {
    if(rd_i_renderer_has_flag(self, RD_RF_NO_SELECTION)) return;

    for(int row = startrow;
        row < (int)vect_length(&self->rows_back) && row <= endrow; row++) {
        RDRow* r = vect_at(&self->rows_back, row);
        int sc = 0, ec = 0;

        if(!rd_i_row_is_empty(r)) ec = rd_i_row_length(r) - 1;
        if(row == startrow) sc = startcol;
        if(row == endrow) ec = endcol;

        for(int col = sc; col < rd_i_row_length(r) && col < ec; col++) {
            rd_i_row_cell_at(r, col)->fg = RD_THEME_SELECTION_FG;
            rd_i_row_cell_at(r, col)->bg = RD_THEME_SELECTION_BG;
        }
    }
}

void rd_i_renderer_new_row(RDRenderer* self, const RDListingItem* item) {
    _rd_renderer_calc_auto_column(self);
    rd_i_rowvect_push(&self->rows_back, self->listing_idx, item);

    if(self->columns)
        rd_i_row_reserve(vect_last(&self->rows_back), self->columns);

    if(!rd_i_renderer_has_flag(self, RD_RF_NO_ADDRESS)) {
        rd_renderer_norm(self, item->segment->base.name);
        rd_renderer_norm(self, ":");

        const unsigned int INT_SIZE = self->context->processorplugin->int_size;

        const unsigned int SIZE =
            item->segment->base.unit ? item->segment->base.unit : INT_SIZE;

        const char* address = rd_i_to_hex((i64)item->address, SIZE);

        rd_renderer_norm(self, address);
        rd_renderer_ws(self, 2);
    }

    if(!rd_i_renderer_has_flag(self, RD_RF_NO_INDENT))
        rd_renderer_ws(self, item->indent);
}

void rd_renderer_text(RDRenderer* self, const char* s, RDThemeKind fg,
                      RDThemeKind bg) {
    assert(s && "invalid chunk string");

    RDRow* r = vect_last(&self->rows_back);
    r->curr_data.group_idx = ++self->group_idx;

    while(*s) {
        if(self->columns && rd_i_row_length(r) >= self->columns) break;

        u32 cp;
        s += rd_i_utf8_decode(s, &cp);

        switch(cp) {
            case '\t':
                rd_i_row_push(r, '\\', fg, bg);
                rd_i_row_push(r, 't', fg, bg);
                break;

            case '\n':
                rd_i_row_push(r, '\\', fg, bg);
                rd_i_row_push(r, 'n', fg, bg);
                break;

            case '\r':
                rd_i_row_push(r, '\\', fg, bg);
                rd_i_row_push(r, 'r', fg, bg);
                break;

            case '\v':
                rd_i_row_push(r, '\\', fg, bg);
                rd_i_row_push(r, 'v', fg, bg);
                break;

            case ' ': {
                // un-data whitespaces
                RDCellData olddata = r->curr_data;
                r->curr_data = rd_i_default_cell_data();
                rd_i_row_push(r, ' ', fg, bg);
                r->curr_data = olddata;
                break;
            }

            default: rd_i_row_push(r, cp, fg, bg); break;
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
    const RDInstructionVect* v =
        rd_il_lift(self->context, item->address, &self->instr_buf);

    rd_i_il_render(self, v);
}

void rd_i_renderer_flags(RDRenderer* self, const RDListingItem* item) {
    static const struct {
        bool (*pred)(const RDFlagsBuffer*, usize);
        const char* name;
        RDThemeKind fg;
    } PREDS[] = {
        {rd_flagsbuffer_has_code, "CODE", RD_THEME_FLAG_CODE},
        {rd_flagsbuffer_has_data, "DATA", RD_THEME_FLAG_DATA},
        {rd_flagsbuffer_has_func, "FUNC", RD_THEME_FUNCTION},
        {rd_flagsbuffer_has_call, "CALL", RD_THEME_CALL},
        {rd_flagsbuffer_has_jump, "JUMP", RD_THEME_JUMP},
        {rd_flagsbuffer_has_cond, "COND", RD_THEME_JUMP_COND},
        {rd_flagsbuffer_has_noret, "NORET", RD_THEME_FAIL},
        {rd_flagsbuffer_has_dslot, "DSLOT", RD_THEME_MUTED},
        {rd_flagsbuffer_has_flow, "FLOW", RD_THEME_FOREGROUND},
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
    usize idx = rd_i_address2index(item->segment, item->address);

    if(rd_i_engine_decode(self->context, item->address, item->segment, idx,
                          &instr)) {
        rd_i_processor_render_instruction(self, &instr);
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
    const RDProcessorPlugin* p = self->context->processorplugin;
    const char* mnem = NULL;

    if(p->get_mnemonic) mnem = p->get_mnemonic(instr, self->context->processor);
    if(!mnem) mnem = rd_i_to_dec(instr->id);

    bool is_def = fg == RD_THEME_DEFAULT;

    switch(instr->flow) {
        case RD_IF_JUMP:
            rd_renderer_text(self, mnem, is_def ? RD_THEME_JUMP : fg,
                             RD_THEME_BACKGROUND);
            break;

        case RD_IF_JUMP_COND:
            rd_renderer_text(self, mnem, is_def ? RD_THEME_JUMP_COND : fg,
                             RD_THEME_BACKGROUND);
            break;

        case RD_IF_CALL:
            rd_renderer_text(self, mnem, is_def ? RD_THEME_CALL : fg,
                             RD_THEME_BACKGROUND);
            break;

        case RD_IF_CALL_COND:
            rd_renderer_text(self, mnem, is_def ? RD_THEME_CALL_COND : fg,
                             RD_THEME_BACKGROUND);
            break;

        case RD_IF_STOP:
            rd_renderer_text(self, mnem, is_def ? RD_THEME_STOP : fg,
                             RD_THEME_BACKGROUND);
            break;

        case RD_IF_NOP:
            rd_renderer_text(self, mnem, is_def ? RD_THEME_MUTED : fg,
                             RD_THEME_BACKGROUND);
            break;

        default:
            rd_renderer_text(self, mnem, is_def ? RD_THEME_FOREGROUND : fg,
                             RD_THEME_BACKGROUND);
            break;
    }
}

void rd_renderer_reg(RDRenderer* self, RDReg reg) {
    const RDProcessorPlugin* p = self->context->processorplugin;
    const char* regname = NULL;

    if(p->get_reg_name)
        regname = p->get_reg_name(reg, self->context->processor);

    if(!regname) regname = rd_i_to_dec(reg);

    rd_renderer_text(self, regname, RD_THEME_REG, RD_THEME_BACKGROUND);
}

void rd_renderer_muted(RDRenderer* self, const char* s) {
    rd_renderer_text(self, s, RD_THEME_MUTED, RD_THEME_BACKGROUND);
}

void rd_renderer_loc(RDRenderer* self, RDAddress address, unsigned int fill,
                     RDNumberFlags flags) {
    if(!rd_i_renderer_has_flag(self, RD_RF_NO_NAMES)) {
        RDName n;
        bool hasname = rd_i_get_name(self->context, address, false, &n);

        if(!hasname) {
            const RDSegmentFull* seg =
                rd_i_db_find_segment(self->context, address);

            if(seg) {
                usize idx = rd_i_address2index(seg, address);

                // auto-generated name with inbound refs/types/functions
                if(rd_i_flagsbuffer_has_xref_in(seg->flags, idx) ||
                   rd_i_flagsbuffer_has_type(seg->flags, idx) ||
                   rd_flagsbuffer_has_func(seg->flags, idx))
                    hasname = rd_i_get_name(self->context, address, true, &n);
            }
        }

        if(hasname) {
            rd_renderer_text(self, n.value, RD_THEME_LOCATION,
                             RD_THEME_BACKGROUND);
            return;
        }
    }

    _rd_renderer_num(self, (i64)address, 16, fill, RD_THEME_NUMBER, flags);
}

void rd_renderer_num(RDRenderer* self, i64 c, unsigned int base,
                     unsigned int fill, RDNumberFlags flags) {
    bool hasname = false;

    if(!rd_i_renderer_has_flag(self, RD_RF_NO_NAMES) &&
       !(flags & RD_NUM_SIGNED) && !(flags & RD_NUM_NOADDR)) {
        hasname = rd_is_address(self->context, (RDAddress)c);
    }

    _rd_renderer_num(self, c, base, fill,
                     hasname ? RD_THEME_LOCATION : RD_THEME_NUMBER, flags);
}

RDContext* rd_renderer_get_context(RDRenderer* self) { return self->context; }

const char* rd_i_renderer_get_text(RDRenderer* self, RDSurfacePos startpos,
                                   RDSurfacePos endpos) {
    usize len = vect_length(&self->rows_front);
    vect_clear(&self->text_buf);

    for(int i = startpos.row; i < (int)len && i <= endpos.row; i++) {
        if(!vect_is_empty(&self->text_buf)) vect_push(&self->text_buf, '\n');

        RDRow* r = vect_at(&self->rows_front, i);
        int s = 0, e = 0;

        if(!rd_i_row_is_empty(r)) e = rd_i_row_length(r) - 1;
        if(i == startpos.row) s = startpos.col;
        if(i == endpos.row) e = endpos.col;

        for(int j = s; j <= e; j++)
            rd_i_cp_push_utf8(&self->text_buf, rd_i_row_cell_at(r, j)->cp);
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
    if(self->hl_word) rd_free(self->hl_word);
    self->hl_word = w && *w ? rd_strdup(w) : NULL;
}

void rd_i_renderer_fit(const RDRenderer* self, int* row, int* col) {
    if(vect_is_empty(&self->rows_front)) {
        *row = 0;
        *col = 0;
        return;
    }

    int nrows = (int)vect_length(&self->rows_front);
    if(*row >= nrows)
        *row = nrows - 1;
    else if(*row < 0)
        *row = 0;

    int ncols = rd_i_row_length(vect_at(&self->rows_front, *row));
    if(!ncols || *col < 0)
        *col = 0;
    else if(*col >= ncols)
        *col = ncols - 1;
}

bool rd_i_renderer_is_index_visible(const RDRenderer* self, LIndex index) {
    if(vect_is_empty(&self->rows_front)) return false;

    const RDRow* first = vect_first(&self->rows_front);
    const RDRow* last = vect_last(&self->rows_front);
    return index >= first->index && index < last->index;
}

bool rd_i_renderer_get_cell_data_under_pos(const RDRenderer* self,
                                           const RDSurfacePos* pos,
                                           RDCellData* cd) {
    if(pos->row >= (int)vect_length(&self->rows_front)) return false;

    RDRow* r = vect_at(&self->rows_front, pos->row);
    if(pos->col >= rd_i_row_length(r)) return false;

    if(cd) *cd = *rd_i_row_data_at(r, pos->col);
    return true;
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
        .data = vect_at(&self->rows_front, idx)->cells.data,
        .length = vect_at(&self->rows_front, idx)->cells.length,
        .content_length = vect_at(&self->rows_front, idx)->content_length,
    };
}

RDCellData* rd_i_renderer_get_current_cell_data(const RDRenderer* self) {
    RDRow* r = vect_last(&self->rows_back);
    return &r->curr_data;
}

void rd_i_renderer_set_current_cell_data(RDRenderer* self, RDCellData m) {
    RDRow* r = vect_last(&self->rows_back);
    r->curr_data = m;
}

usize rd_i_renderer_get_row_count(const RDRenderer* self) {
    return vect_length(&self->rows_front);
}

bool rd_i_renderer_select_word(RDRenderer* self, int row, int col,
                               RDSurfacePos* startpos, RDSurfacePos* endpos) {
    if(row >= (int)vect_length(&self->rows_front)) return false;

    RDRow* r = vect_at(&self->rows_front, row);
    if(col >= rd_i_row_length(r)) col = rd_i_row_length(r) - 1;

    unsigned int group_idx = rd_i_row_data_at(r, col)->group_idx;
    if(!group_idx) return false;

    int startcol = 0, endcol = 0;

    for(int i = col; i-- > 0;) {
        if(rd_i_row_data_at(r, i)->group_idx != group_idx) {
            startcol = i + 1;
            break;
        }
    }

    for(int i = col; i < rd_i_row_length(r); i++) {
        if(rd_i_row_data_at(r, i)->group_idx != group_idx) {
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
    int cols = self->columns;
    vect_clear(v);

    if(cols && (int)vect_capacity(v) < cols)
        vect_reserve(v,
                     (usize)(cols * (int)vect_length(&self->rows_front)) + 1);

    const RDRow* r;
    vect_each(r, &self->rows_front) {
        if(!vect_is_empty(v)) vect_push(v, '\n');

        const RDCell* c;
        vect_each(c, &r->cells) rd_i_cp_push_utf8(v, c->cp);
    }

    vect_push(v, 0);
}
