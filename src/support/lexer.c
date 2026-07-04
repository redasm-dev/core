#include "lexer.h"
#include "support/containers.h"
#include <ctype.h>
#include <redasm/allocator.h>
#include <redasm/support/logging.h>

#define _rd_lexer_tokenize(self, tokentype, cond, t)                           \
    do {                                                                       \
        t->type = tokentype;                                                   \
        t->value = self->curr;                                                 \
        t->line = self->line;                                                  \
        t->col = self->col;                                                    \
        t->pos = self->pos;                                                    \
        while(cond) {                                                          \
            _rd_lexer_get(self);                                               \
        }                                                                      \
        t->length = (usize)(self->curr - t->value);                            \
    } while(0)

static bool _rd_lexer_is_number_or_real(char c, RDToken* t) {
    if(c == '.') {
        if(t->type == RD_TOK_NUMBER_REAL) return false;

        t->type = RD_TOK_NUMBER_REAL;
        return true;
    }

    return isdigit(c);
}

static char _rd_lexer_get(RDLexer* self) {
    char c = *self->curr++;
    self->pos++;

    if(c == '\n') {
        self->line++;
        self->col = 0;
    }
    else
        self->col++;

    return c;
}

static void _rd_lexer_atomize(RDLexer* self, RDTokenType type, RDToken* t) {
    t->type = type;
    t->value = self->curr++;
    t->length = 1;
    t->line = self->line;
    t->col = self->col;
    t->pos = self->pos;
}

static bool _rd_lexer_stop(RDLexer* self, RDTokenType type, RDToken* t) {
    t->type = type;
    t->value = self->curr ? self->curr : "";
    t->length = self->curr ? 1 : 0;
    t->line = self->line;
    t->col = self->col;
    t->pos = self->pos;
    return false;
}

static bool _rd_lexer_identifier(RDLexer* self, RDToken* t) {
    _rd_lexer_tokenize(self, RD_TOK_IDENTIFIER,
                       (isalnum(*self->curr) || *self->curr == '_'), t);
    return true;
}

static bool _rd_lexer_hexnumber(RDLexer* self, RDToken* t) {
    // save pointer & position information
    const char* p = self->curr;
    usize line = self->line, col = self->col, pos = self->pos;
    _rd_lexer_get(self); // eat '0'
    _rd_lexer_get(self); // eat 'x'

    _rd_lexer_tokenize(self, RD_TOK_NUMBER, (isxdigit(*self->curr)), t);

    t->value = p;
    t->length += 2;
    t->line = line;
    t->col = col;
    t->pos = pos;

    if(isalpha(*self->curr) || *self->curr == '_') {
        const char* extra = self->curr;
        while(isalnum(*self->curr) || *self->curr == '_')
            _rd_lexer_get(self);
        t->length += (usize)(self->curr - extra);
        t->type = RD_TOK_UNEXPECTED;
        return false;
    }

    // "0x" prefix in t->value is handled by base 16 automatically
    t->u_value = (u64)strtoull(t->value, NULL, 16);
    return true;
}

static bool _rd_lexer_number(RDLexer* self, RDToken* t) {
    _rd_lexer_tokenize(self, RD_TOK_NUMBER,
                       (_rd_lexer_is_number_or_real(*self->curr, t)), t);

    // malformed: digits/real running directly into more literal-like
    // characters with no separator "3eax", "12.34.56"
    // never silently split into multiple valid tokens.
    bool second_dot = t->type == RD_TOK_NUMBER_REAL && *self->curr == '.';

    // only reject if what follows could itself continue as an identifier.
    // punctuation, whitespace, and NUL are all valid, unambiguous stops.
    if(isalpha(*self->curr) || *self->curr == '_' || second_dot) {
        const char* extra = self->curr;
        while(isalnum(*self->curr) || *self->curr == '_' || *self->curr == '.')
            _rd_lexer_get(self);
        t->type = RD_TOK_UNEXPECTED;
        t->length += (usize)(self->curr - extra);
        return false;
    }

    if(t->type == RD_TOK_NUMBER_REAL)
        t->r_value = strtod(t->value, NULL);
    else
        t->u_value = (u64)strtoull(t->value, NULL, 10);

    return true;
}

static bool _rd_lexer_punct(RDLexer* self, RDToken* t) {
    switch(*self->curr) {
        case '!': _rd_lexer_atomize(self, RD_TOK_EXCLAMATION, t); break;
        case '\"': _rd_lexer_atomize(self, RD_TOK_DOUBLE_QUOTE, t); break;
        case '#': _rd_lexer_atomize(self, RD_TOK_HASH, t); break;
        case '$': _rd_lexer_atomize(self, RD_TOK_DOLLAR, t); break;
        case '%': _rd_lexer_atomize(self, RD_TOK_PERCENT, t); break;
        case '&': _rd_lexer_atomize(self, RD_TOK_AMPERSAND, t); break;
        case '\'': _rd_lexer_atomize(self, RD_TOK_SINGLE_QUOTE, t); break;
        case '(': _rd_lexer_atomize(self, RD_TOK_OPEN_ROUND, t); break;
        case ')': _rd_lexer_atomize(self, RD_TOK_CLOSE_ROUND, t); break;
        case '*': _rd_lexer_atomize(self, RD_TOK_ASTERISK, t); break;
        case '+': _rd_lexer_atomize(self, RD_TOK_PLUS, t); break;
        case ',': _rd_lexer_atomize(self, RD_TOK_COMMA, t); break;
        case '-': _rd_lexer_atomize(self, RD_TOK_MINUS, t); break;
        case '.': _rd_lexer_atomize(self, RD_TOK_DOT, t); break;
        case '/': _rd_lexer_atomize(self, RD_TOK_SLASH, t); break;
        case ':': _rd_lexer_atomize(self, RD_TOK_COLON, t); break;
        case ';': _rd_lexer_atomize(self, RD_TOK_SEMICOLON, t); break;
        case '<': _rd_lexer_atomize(self, RD_TOK_LESS_THAN, t); break;
        case '=': _rd_lexer_atomize(self, RD_TOK_EQUAL, t); break;
        case '>': _rd_lexer_atomize(self, RD_TOK_GREATER_THAN, t); break;
        case '?': _rd_lexer_atomize(self, RD_TOK_QUESTION, t); break;
        case '@': _rd_lexer_atomize(self, RD_TOK_AT, t); break;
        case '[': _rd_lexer_atomize(self, RD_TOK_OPEN_SQUARE, t); break;
        case '\\': _rd_lexer_atomize(self, RD_TOK_BACKSLASH, t); break;
        case ']': _rd_lexer_atomize(self, RD_TOK_CLOSE_SQUARE, t); break;
        case '^': _rd_lexer_atomize(self, RD_TOK_CIRCUMFLEX, t); break;
        case '_': _rd_lexer_atomize(self, RD_TOK_UNDERSCORE, t); break;
        case '`': _rd_lexer_atomize(self, RD_TOK_BACK_QUOTE, t); break;
        case '{': _rd_lexer_atomize(self, RD_TOK_OPEN_CURLY, t); break;
        case '|': _rd_lexer_atomize(self, RD_TOK_PIPE, t); break;
        case '}': _rd_lexer_atomize(self, RD_TOK_CLOSE_CURLY, t); break;
        case '~': _rd_lexer_atomize(self, RD_TOK_TILDE, t); break;
        default: return _rd_lexer_stop(self, RD_TOK_UNEXPECTED, t);
    }

    return true;
}

RDLexer* rd_lexer_create(const char* s) {
    RDLexer* self = rd_alloc(sizeof(*self));

    *self = (RDLexer){
        .input = s,
        .curr = s,
    };

    return self;
}

void rd_lexer_destroy(RDLexer* self) {
    if(!self) return;

    vect_destroy(&self->token_value_buf);
    rd_free(self);
}

bool rd_lexer_next(RDLexer* self, RDToken* t) {
    if(!t) return false;
    if(!self->curr) return _rd_lexer_stop(self, RD_TOK_END, t);

    while(isspace(*self->curr))
        _rd_lexer_get(self);

    if(isalpha(*self->curr) ||
       (*self->curr == '_' &&
        (isalnum(*(self->curr + 1)) || *(self->curr + 1) == '_'))) {
        return _rd_lexer_identifier(self, t);
    }

    if(isdigit(*self->curr)) {
        switch(*(self->curr + 1)) {
            case 'x':
            case 'X': return _rd_lexer_hexnumber(self, t);

            default: return _rd_lexer_number(self, t);
        }
    }

    if(ispunct(*self->curr)) return _rd_lexer_punct(self, t);
    if(*self->curr) return _rd_lexer_stop(self, RD_TOK_UNEXPECTED, t);
    return _rd_lexer_stop(self, RD_TOK_END, t);
}

bool rd_lexer_next_expect(RDLexer* self, RDTokenType type, RDToken* t) {
    if(!t) return false;
    rd_lexer_next(self, t);
    return t->type == type;
}

void rd_lexer_reset(RDLexer* self, const char* s) {
    self->input = s;
    self->curr = s;
    self->line = 0;
    self->col = 0;
    self->pos = 0;
}

RDLexerMark rd_lexer_save(const RDLexer* self) {
    return (RDLexerMark){
        .curr = self->curr,
        .pos = self->pos,
        .col = self->col,
        .line = self->line,
    };
}

void rd_lexer_restore(RDLexer* self, RDLexerMark mark) {
    usize len = self->input ? strlen(self->input) : 0;

    if(mark.curr < self->input || mark.curr > self->input + len) {
        RD_LOG_FAIL("rd_lexer_restore: mark does not belong to this lexer's "
                    "current input");
        return;
    }

    self->curr = mark.curr;
    self->line = mark.line;
    self->col = mark.col;
    self->pos = mark.pos;
}

bool rd_lexer_peek(RDLexer* self, RDToken* t) {
    RDLexerMark mark = rd_lexer_save(self);
    bool ok = rd_lexer_next(self, t);
    rd_lexer_restore(self, mark);
    return ok;
}

bool rd_lexer_peek_expect(RDLexer* self, RDTokenType type, RDToken* t) {
    if(!t) return false;
    rd_lexer_peek(self, t);
    return t->type == type;
}

bool rd_lexer_consume(RDLexer* self) {
    RDToken t;
    return rd_lexer_next(self, &t);
}

bool rd_lexer_at_end(RDLexer* self) {
    RDToken t;
    return rd_lexer_peek_expect(self, RD_TOK_END, &t);
}

bool rd_lexer_try_any(RDLexer* self, const RDLexerTryFn* fns, void* userdata) {
    while(*fns) {
        RDLexerMark m = rd_lexer_save(self);
        if((*fns)(self, userdata)) return true;
        rd_lexer_restore(self, m);
        fns++;
    }

    return false;
}

const char* rd_lexer_token_value(RDLexer* self, const RDToken* t) {
    str_clear(&self->token_value_buf);
    str_append_n(&self->token_value_buf, t->value, t->length);
    return self->token_value_buf.data;
}
