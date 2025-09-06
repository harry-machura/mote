#include <stdio.h>
#include "motec_core.h"
#include "motec_additions.h"

// ==== Hilfsfunktionen aus motec.c benutzen ====
// In motec.c gibt es:
//   static int match(Lex* L, TokType t);
//   static void expect(Lex* L, TokType t);
//   static void advance(Lex* L);
//   static void parse_expr(P* p);
//   static void parse_stmt(P* p);
//   static void parse_call_and_emit(P* p, const char* name);
//   static uint8_t sym_get_slot(SymTab* T, const char* name);
//
// Wir rufen sie direkt auf, NICHT als extern, weil sie dort schon definiert sind.

// Dummy-Helfer, weil sie im Original fehlen:
static int parse_int_literal(Lex* L, int* out) {
    if (L->cur.t == T_NUMBER) {
        *out = L->cur.ival;
        advance(L);
        return 1;
    }
    return 0;
}

static int is_identifier_token(int tok) {
    return tok == T_IDENT;
}

static void parse_file_into(P* p, const char* path) {
    fprintf(stderr, "import nicht implementiert: %s\n", path);
}

// ===== Break/Continue Stack Helpers =====
static void bc_push(P* p, int break_target_unused, int continue_target) {
    if (p->bc_sp < 64) {
        p->break_stack[p->bc_sp]    = -1;              // Kopf der break-Patchkette
        p->continue_stack[p->bc_sp] = continue_target; // Ziel für continue
        p->bc_sp++;
    }
}

static void bc_pop(P* p) { if (p->bc_sp > 0) p->bc_sp--; }

static inline int32_t read_i32(Buf* b, size_t at){ int32_t v; memcpy(&v,b->data+at,4); return v; }
static inline void    write_i32(Buf* b, size_t at, int32_t v){ memcpy(b->data+at,&v,4); }

// ===== Kontrollstrukturen =====
void parse_if(P* p) {
    //expect(&p->L, T_IF);
    expect(&p->L, T_LPAREN);
    parse_expr(p);
    expect(&p->L, T_RPAREN);

    int jz_else = p->out->len;
    emit_op(p->out, OP_JZ);
    emiti32(p->out, 0);

    expect(&p->L, T_LBRACE);
    while (p->L.cur.t != T_RBRACE && p->L.cur.t != T_EOF) {
        parse_stmt(p);
    }
    expect(&p->L, T_RBRACE);

    int jmp_end = 0;
    if (p->L.cur.t == T_ELSE) {
        advance(&p->L);
        jmp_end = p->out->len;
        emit_op(p->out, OP_JMP);
        emiti32(p->out, 0);

        int pc = p->out->len;
        unsigned char* buf = &p->out->data[jz_else+1];
        buf[0] = (pc & 0xFF);
        buf[1] = (pc >> 8) & 0xFF;
        buf[2] = (pc >> 16) & 0xFF;
        buf[3] = (pc >> 24) & 0xFF;

        expect(&p->L, T_LBRACE);
        while (p->L.cur.t != T_RBRACE && p->L.cur.t != T_EOF) {
            parse_stmt(p);
        }
        expect(&p->L, T_RBRACE);
    }

    int pc = p->out->len;
    unsigned char* buf = &p->out->data[jz_else+1];
    buf[0] = (pc & 0xFF);
    buf[1] = (pc >> 8) & 0xFF;
    buf[2] = (pc >> 16) & 0xFF;
    buf[3] = (pc >> 24) & 0xFF;

    if (jmp_end) {
        unsigned char* buf2 = &p->out->data[jmp_end+1];
        buf2[0] = (pc & 0xFF);
        buf2[1] = (pc >> 8) & 0xFF;
        buf2[2] = (pc >> 16) & 0xFF;
        buf2[3] = (pc >> 24) & 0xFF;
    }
}

void parse_while(P* p) {
    printf("DEBUG: parse_while() gestartet, aktuelles Token: %d ('%s')\n", 
           p->L.cur.t, p->L.cur.s);
    
    int loop_start = p->out->len;
    
    printf("DEBUG: Erwarte LPAREN, aktuelles Token: %d ('%s')\n", 
           p->L.cur.t, p->L.cur.s);
    
    expect(&p->L, T_LPAREN);
    parse_expr(p);
    expect(&p->L, T_RPAREN);

    int jz_end = p->out->len;
    emit_op(p->out, OP_JZ);
    emiti32(p->out, 0);

    bc_push(p, jz_end, loop_start);

    expect(&p->L, T_LBRACE);
    while (p->L.cur.t != T_RBRACE && p->L.cur.t != T_EOF) {
        parse_stmt(p);
    }
    expect(&p->L, T_RBRACE);

    emit_op(p->out, OP_JMP);
    emiti32(p->out, loop_start);

    int pc = p->out->len;
    unsigned char* buf = &p->out->data[jz_end+1];
    buf[0] = (pc & 0xFF);
    buf[1] = (pc >> 8) & 0xFF;
    buf[2] = (pc >> 16) & 0xFF;
    buf[3] = (pc >> 24) & 0xFF;

    bc_pop(p);
}

void parse_break(P* p) {
    if (p->bc_sp <= 0) { fprintf(stderr,"break außerhalb einer Schleife\n"); return; }
    int head = p->break_stack[p->bc_sp - 1];
    int patch_pc = p->out->len;
    emit_op(p->out, OP_JMP);
    emiti32(p->out, head);                 // next = alter Kopf
    p->break_stack[p->bc_sp - 1] = patch_pc; // neuer Kopf
}


void parse_continue(P* p) {
    //expect(&p->L, T_CONTINUE);
    if (p->bc_sp <= 0) {
        fprintf(stderr, "continue außerhalb einer Schleife\n");
        return;
    }
    int target = p->continue_stack[p->bc_sp - 1];
    emit_op(p->out, OP_JMP);
    emiti32(p->out, target);
}


void parse_import(P* p) {
    if (p->L.cur.t == T_IDENT) {
        const char* id = p->L.cur.s;
        parse_file_into(p, id);
        advance(&p->L);
    } else {
        fprintf(stderr, "import erwartet Identifier\n");
    }
}

static void parse_assignment_or_call_expr(P* p) {
    if (p->L.cur.t == T_IDENT) {
        const char* name = p->L.cur.s;
        advance(&p->L);
        if (p->L.cur.t == T_ASSIGN) {
            advance(&p->L);
            parse_expr(p);
            uint8_t slot = sym_get_slot(&p->syms, name);
            emit_op(p->out, OP_STOREL);
            emit8(p->out, slot);
            return;
        } else if (p->L.cur.t == T_LPAREN) {
            parse_call_and_emit(p, name);
            return;
        } else {
            uint8_t slot = sym_get_slot(&p->syms, name);
            emit_op(p->out, OP_LOADL);
            emit8(p->out, slot);
            return;
        }
    }
    parse_expr(p);
}

void parse_let_stmt(P* p) {
     // 'let' wurde in motec.c bereits via advance() konsumiert
    if (p->L.cur.t != T_IDENT) {
        fprintf(stderr, "let: Variablenname erwartet\n");
        exit(2);
    }
    char name[64];
    strncpy(name, p->L.cur.s, sizeof(name));
    name[63] = 0;
    advance(&p->L);

    expect(&p->L, T_ASSIGN);
    parse_expr(p);

    uint8_t slot = sym_get_slot(&p->syms, name);
    emit_op(p->out, OP_STOREL);
    emit8(p->out, slot);

    expect(&p->L, T_SEMI);
}


void parse_assignment_or_call_stmt(P* p) {
    if (p->L.cur.t == T_IDENT) {
        const char* name = p->L.cur.s;
        advance(&p->L);
        if (p->L.cur.t == T_ASSIGN) {
            advance(&p->L);
            parse_expr(p);
            expect(&p->L, T_SEMI);
            uint8_t slot = sym_get_slot(&p->syms, name);
            emit_op(p->out, OP_STOREL);
            emit8(p->out, slot);
            return;
        } else {
            parse_call_and_emit(p, name);
            expect(&p->L, T_SEMI);
            return;
        }
    }
    parse_expr(p);
    expect(&p->L, T_SEMI);
}

// Neuer parse_for: unterstützt init ; cond ; post
void parse_for(P* p) {
    // 'for' wurde in parse_stmt() bereits via advance() konsumiert
    expect(&p->L, T_LPAREN);

    // --- Init ---
    if (p->L.cur.t != T_SEMI) {
        if (p->L.cur.t == T_LET) {
            advance(&p->L);
            if (p->L.cur.t != T_IDENT) {
                fprintf(stderr, "for: Variablenname erwartet\n");
                exit(2);
            }
            char name[64];
            strncpy(name, p->L.cur.s, sizeof(name));
            name[63] = 0;
            advance(&p->L);
            expect(&p->L, T_ASSIGN);
            parse_expr(p); // Startwert
            uint8_t slot = sym_get_slot(&p->syms, name);
            emit_op(p->out, OP_STOREL);
            emit8(p->out, slot);
        } else {
            // Expr statt Stmt (keine Semikolon-Pflicht hier)
            parse_assignment_or_call_expr(p);
        }
    }
    expect(&p->L, T_SEMI);

    // --- Cond ---
    int cond_pc = p->out->len;
    if (p->L.cur.t != T_SEMI) {
        parse_expr(p);
    } else {
        emit_op(p->out, OP_PUSHI);
        emiti32(p->out, 1); // true
    }
    expect(&p->L, T_SEMI);

    // Jump raus, wenn false
    int jz_pc = p->out->len;
    emit_op(p->out, OP_JZ);
    emiti32(p->out, 0); // Patch später

    // --- Post vorbereiten ---
    int post_start = -1, post_len = 0;
    if (p->L.cur.t != T_RPAREN) {
        int save_pc = p->out->len;
        parse_assignment_or_call_expr(p); // kein Semikolon
        post_start = save_pc;
        post_len   = p->out->len - save_pc;
        p->out->len = save_pc; // verwerfen, später wieder einfügen
    }

    expect(&p->L, T_RPAREN);

    // --- Body ---
    // continue -> zurück zur Bedingung (vereinfachte Semantik)
    bc_push(p, jz_pc, cond_pc);

    expect(&p->L, T_LBRACE);
    while (p->L.cur.t != T_RBRACE && p->L.cur.t != T_EOF) {
        parse_stmt(p);
    }
    expect(&p->L, T_RBRACE);

    // --- Post-Teil wieder einfügen ---
    if (post_start >= 0 && post_len > 0) {
        buf_append(p->out, p->out->data + post_start, post_len);
    }

    // --- Zurück zur Condition ---
    emit_op(p->out, OP_JMP);
    emiti32(p->out, cond_pc);

    // --- Patch JZ ---
    int end_pc = p->out->len;
    unsigned char* buf = &p->out->data[jz_pc + 1];
    buf[0] = (end_pc & 0xFF);
    buf[1] = (end_pc >> 8) & 0xFF;
    buf[2] = (end_pc >> 16) & 0xFF;
    buf[3] = (end_pc >> 24) & 0xFF;

    // --- NEU: gesamte 'break'-Kette auf end_pc patchen
    int cur = p->break_stack[p->bc_sp - 1];
    while (cur != -1) {
        int next = read_i32(p->out, cur + 1); // alten 'next' lesen
        write_i32(p->out, cur + 1, end_pc);   // jetzt auf Endadresse zeigen lassen
        cur = next;
    }

    // Nur einmal poppen — nach allem Patchen
    bc_pop(p);
}





void parse_do_while(P* p) {
    fprintf(stderr, "parse_do_while: noch nicht implementiert\n");
}

void parse_switch(P* p) {
    fprintf(stderr, "parse_switch: noch nicht implementiert\n");
}