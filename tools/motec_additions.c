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
static void bc_push(P* p, int break_target, int continue_target) {
    if (p->bc_sp < 64) {
        p->break_stack[p->bc_sp] = break_target;
        p->continue_stack[p->bc_sp] = continue_target;
        p->bc_sp++;
    }
}

static void bc_pop(P* p) {
    if (p->bc_sp > 0) p->bc_sp--;
}

// ===== Kontrollstrukturen =====
void parse_if(P* p) {
    expect(&p->L, T_IF);
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
    expect(&p->L, T_WHILE);
    int loop_start = p->out->len;

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
    if (p->bc_sp <= 0) {
        fprintf(stderr, "break außerhalb von Loop/Switch\n");
        return;
    }
    int jmp = p->out->len;
    emit_op(p->out, OP_JMP);
    emiti32(p->out, 0);

    // sofort patchen: break springt zum gespeicherten Endlabel
    int target = p->break_stack[p->bc_sp-1];
    unsigned char* buf = &p->out->data[jmp+1];
    buf[0] = (target & 0xFF);
    buf[1] = (target >> 8) & 0xFF;
    buf[2] = (target >> 16) & 0xFF;
    buf[3] = (target >> 24) & 0xFF;
}

void parse_continue(P* p) {
    if (p->bc_sp <= 0) {
        fprintf(stderr, "continue außerhalb von Loop\n");
        return;
    }
    emit_op(p->out, OP_JMP);
    emiti32(p->out, p->continue_stack[p->bc_sp-1]);
}

void parse_import(P* p) {
    expect(&p->L, T_IMPORT);
    if (p->L.cur.t == T_IDENT) {
        const char* id = p->L.cur.s;
        parse_file_into(p, id);
        advance(&p->L);
    } else {
        fprintf(stderr, "import erwartet Identifier\n");
    }
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

void parse_for(P* p) {
    expect(&p->L, T_FOR);
    expect(&p->L, T_LPAREN);

    // --- Initialisierung ---
    if (p->L.cur.t == T_LET) {
        // Deklaration erlaubt: for (let i = 0; ...
        advance(&p->L);
        if (p->L.cur.t != T_IDENT) {
            fprintf(stderr, "for: erwartet Variablennamen nach let\n");
            return;
        }
        const char* name = p->L.cur.s;
        advance(&p->L);
        expect(&p->L, T_ASSIGN);
        parse_expr(p);
        expect(&p->L, T_SEMI);
        uint8_t slot = sym_get_slot(&p->syms, name);
        emit_op(p->out, OP_STOREL);
        emit8(p->out, slot);
    } else if (p->L.cur.t != T_SEMI) {
        // normale Zuweisung: i = 0;
        parse_assignment_or_call_stmt(p);
    } else {
        // kein Init
        expect(&p->L, T_SEMI);
    }

    // --- Bedingung ---
    int cond_start = p->out->len;
    if (p->L.cur.t != T_SEMI) {
        parse_expr(p);
    } else {
        // leere Bedingung gilt als true
        emit_op(p->out, OP_PUSHI);
        emiti32(p->out, 1);
    }
    expect(&p->L, T_SEMI);

    int jz_end = p->out->len;
    emit_op(p->out, OP_JZ);
    emiti32(p->out, 0);

    // --- Post-Anweisung merken ---
    int post_pos = p->L.cur.t != T_RPAREN ? p->L.i : -1;
    char post_buf[128];
    if (p->L.cur.t != T_RPAREN) {
        // wir nehmen den Identifier für i = i + 1
        if (p->L.cur.t == T_IDENT) {
            strncpy(post_buf, p->L.cur.s, sizeof(post_buf));
            post_buf[sizeof(post_buf)-1] = 0;
        }
        // alles bis ')' überspringen
        while (p->L.cur.t != T_RPAREN && p->L.cur.t != T_EOF) {
            advance(&p->L);
        }
    }
    expect(&p->L, T_RPAREN);

    // --- Body ---
    bc_push(p, jz_end, cond_start);
    expect(&p->L, T_LBRACE);
    while (p->L.cur.t != T_RBRACE && p->L.cur.t != T_EOF) {
        parse_stmt(p);
    }
    expect(&p->L, T_RBRACE);
    bc_pop(p);

    // --- Post (z. B. i = i + 1) ---
    if (post_pos >= 0) {
        // hier könntest du später den Post-Teil richtig parsen
        // fürs Erste: noop
    }

    // zurück zum Anfang
    emit_op(p->out, OP_JMP);
    emiti32(p->out, cond_start);

    // --- Sprung hinter die Schleife patchen ---
    int pc = p->out->len;
    unsigned char* buf = &p->out->data[jz_end+1];
    buf[0] = (pc & 0xFF);
    buf[1] = (pc >> 8) & 0xFF;
    buf[2] = (pc >> 16) & 0xFF;
    buf[3] = (pc >> 24) & 0xFF;
}


void parse_do_while(P* p) {
    fprintf(stderr, "parse_do_while: noch nicht implementiert\n");
}

void parse_switch(P* p) {
    fprintf(stderr, "parse_switch: noch nicht implementiert\n");
}
