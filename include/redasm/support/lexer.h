#pragma once

#include <redasm/config.h>

// clang-format off
typedef enum {
    RD_TOK_END = 0,
    RD_TOK_UNEXPECTED,
    RD_TOK_IDENTIFIER,
    RD_TOK_NUMBER,       // .u_value/.s_value/.is_signed resolved, any radix
    RD_TOK_NUMBER_REAL,  // recognized, not consumed by encode's v1 grammar
    RD_TOK_OPEN_ROUND,    RD_TOK_CLOSE_ROUND,
    RD_TOK_OPEN_SQUARE,   RD_TOK_CLOSE_SQUARE,
    RD_TOK_OPEN_CURLY,    RD_TOK_CLOSE_CURLY,
    RD_TOK_LESS_THAN,     RD_TOK_GREATER_THAN,
    RD_TOK_EQUAL,         RD_TOK_EXCLAMATION,   RD_TOK_QUESTION,
    RD_TOK_DOLLAR,        RD_TOK_PERCENT,       RD_TOK_AMPERSAND,
    RD_TOK_PLUS,          RD_TOK_MINUS,         RD_TOK_ASTERISK,
    RD_TOK_BACKSLASH,     RD_TOK_SLASH,         RD_TOK_HASH,
    RD_TOK_DOT,           RD_TOK_COMMA,         RD_TOK_COLON,
    RD_TOK_SEMICOLON,     RD_TOK_UNDERSCORE,    RD_TOK_CIRCUMFLEX,    
    RD_TOK_PIPE,          RD_TOK_TILDE,         RD_TOK_SINGLE_QUOTE,  
    RD_TOK_DOUBLE_QUOTE,  RD_TOK_BACK_QUOTE,    RD_TOK_AT,
} RDTokenType;
// clang-format on

typedef struct RDLexer RDLexer;

typedef bool (*RDLexerTryFn)(RDLexer*, void*);

typedef struct RDLexerMark {
    const char* curr;
    usize line, col, pos;
} RDLexerMark;

typedef struct RDToken {
    RDTokenType type;
    const char* value; // non-owning span into the lexer's source text
    usize length, line, col, pos;

    union {
        u64 u_value;
        double r_value;
    }; // valid only for RD_TOK_NUMBER
} RDToken;

// clang-format off
RD_API RDLexer* rd_lexer_create(const char* s);
RD_API void rd_lexer_destroy(RDLexer* self);
RD_API void rd_lexer_reset(RDLexer* self, const char* s);
RD_API void rd_lexer_set_default_base(RDLexer* self, int base);
RD_API bool rd_lexer_next(RDLexer* self, RDToken* t);
RD_API bool rd_lexer_next_expect(RDLexer* self, RDTokenType type, RDToken* t);
RD_API bool rd_lexer_peek(RDLexer* self, RDToken* t);
RD_API bool rd_lexer_peek_expect(RDLexer* self, RDTokenType type, RDToken* t);
RD_API bool rd_lexer_consume(RDLexer* self);
RD_API bool rd_lexer_try_consume(RDLexer* self, RDTokenType type, RDToken* t);
RD_API bool rd_lexer_try_any(RDLexer* self, const RDLexerTryFn* fns, void* userdata);
RD_API bool rd_lexer_at_end(RDLexer* self);
RD_API RDLexerMark rd_lexer_save(const RDLexer* self);
RD_API void rd_lexer_restore(RDLexer* self, RDLexerMark mark);
RD_API const char* rd_lexer_token_value(RDLexer* self, const RDToken* t);
// clang-format on
