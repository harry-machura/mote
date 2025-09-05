#ifndef MOTEC_CORE_H
#define MOTEC_CORE_H

#include <stdint.h>

// ---- Bytecode Opcodes ----
typedef enum {
    OP_HALT=0, OP_PUSHI=1, OP_LOADL=2, OP_STOREL=3,
    OP_ADD=4, OP_SUB=5, OP_MUL=6, OP_DIV=7,
    OP_JMP=8, OP_JZ=9, OP_CALL=10,
    OP_LT=11, OP_EQ=12,
    OP_DUP=13, OP_DROP=14, OP_SWAP=15, OP_OVER=16,
    OP_GT=17, OP_GE=18, OP_LE=19, OP_NE=20,
    OP_NOT=21, OP_AND=22, OP_OR=23,
    OP_CALLUSER=24, OP_RET=25
} Op;

// ---- Bytebuffer ----
typedef struct { uint8_t *data; size_t len, cap; } Buf;

// ---- Lexer ----
typedef enum {
    T_EOF=0, T_NUMBER, T_IDENT,
    T_EQEQ, T_LT, T_PLUS, T_MINUS, T_STAR, T_SLASH,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE,
    T_SEMI, T_ASSIGN, T_COMMA, T_COLON,
    T_OROR, T_ANDAND, T_GT, T_GE, T_LE, T_BANG, T_NEQ,
    T_FUNC, T_RETURN,
    // NEU:
    T_IF, T_ELSE, T_WHILE, T_FOR, T_DO,
    T_SWITCH, T_CASE, T_DEFAULT,
    T_BREAK, T_CONTINUE, T_IMPORT, T_LET
} TokType;

typedef struct { TokType t; char s[128]; int ival; } Tok;
typedef struct { const char *src; size_t i, n; Tok cur; } Lex;

// ---- Symboltabellen ----
typedef struct { char name[64]; uint8_t slot; } Sym;
typedef struct { Sym a[256]; int n; } SymTab;

// ---- Parser ----
typedef struct {
    Lex L;
    SymTab syms;
    Buf *out;
    // NEU für break/continue:
    int break_stack[64];
    int continue_stack[64];
    int bc_sp;
} P;

#endif