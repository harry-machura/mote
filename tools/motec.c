// motec.c – Compiler für die Mote-VM mit Funktionsunterstützung
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "motec_core.h"
#include "motec_additions.h"

// ---- Bytebuffer Funktionen ----
void buf_init(Buf *b){ b->data=NULL; b->len=0; b->cap=0; }
void buf_res(Buf *b, size_t need){
    if (need>b->cap){
        size_t nc = b->cap? b->cap*2:256;
        while(nc<need) nc*=2;
        b->data=(uint8_t*)realloc(b->data,nc);
        b->cap=nc;
    }
}

void buf_copy(Buf* dst, Buf* src, size_t offset, size_t length) {
    buf_res(dst, dst->len + length);
    memcpy(dst->data + dst->len, src->data + offset, length);
    dst->len += length;
}

void buf_append(Buf* dst, const uint8_t* data, size_t length) {
    buf_res(dst, dst->len + length);
    memcpy(dst->data + dst->len, data, length);
    dst->len += length;
}

void buf_free(Buf* b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void emit8(Buf*b, uint8_t v){ buf_res(b, b->len+1); b->data[b->len++]=v; }
void emiti32(Buf*b, int32_t v){ buf_res(b, b->len+4); memcpy(b->data+b->len,&v,4); b->len+=4; }
static void patch_i32(Buf*b, size_t at, int32_t v){ memcpy(b->data+at,&v,4); }
void emit_op(Buf*b, Op op){ emit8(b,(uint8_t)op); }

// ---- Lexer ----
static int is_ident_start(int c){ return isalpha(c) || c=='_'; }
static int is_ident_body (int c){ return isalnum(c) || c=='_'; }

static void skip_ws_comments(Lex *L){
    for(;;){
        while (L->i<L->n && strchr(" \t\r\n",L->src[L->i])) L->i++;
        if (L->i+1<L->n && L->src[L->i]=='/' && L->src[L->i+1]=='/'){
            L->i+=2; while(L->i<L->n && L->src[L->i]!='\n') L->i++; continue;
        }
        if (L->i+1<L->n && L->src[L->i]=='/' && L->src[L->i+1]=='*'){
            L->i+=2; while(L->i+1<L->n && !(L->src[L->i]=='*'&&L->src[L->i+1]=='/')) L->i++;
            if (L->i+1<L->n) L->i+=2; continue;
        }
        break;
    }
}

static Tok lex_next(Lex *L){
    skip_ws_comments(L);
    Tok out; memset(&out,0,sizeof(out));
    if (L->i>=L->n){ out.t=T_EOF; return out; }
    char c = L->src[L->i++];
    switch(c){
        case '+': out.t=T_PLUS; return out;
        case '-': out.t=T_MINUS; return out;
        case '*': out.t=T_STAR; return out;
        case '/': out.t=T_SLASH; return out;
        case '(': out.t=T_LPAREN; return out;
        case ')': out.t=T_RPAREN; return out;
        case '{': out.t=T_LBRACE; return out;
        case '}': out.t=T_RBRACE; return out;
        case ';': out.t=T_SEMI; return out;
        case ',': out.t=T_COMMA; return out;
        case ':': out.t=T_COLON; return out;
        case '=': if (L->i<L->n && L->src[L->i]=='='){ L->i++; out.t=T_EQEQ; } else out.t=T_ASSIGN; return out;
        case '<': if (L->i<L->n && L->src[L->i]=='='){ L->i++; out.t=T_LE; } else out.t=T_LT; return out;
        case '>': if (L->i<L->n && L->src[L->i]=='='){ L->i++; out.t=T_GE; } else out.t=T_GT; return out;
        case '!': if (L->i<L->n && L->src[L->i]=='='){ L->i++; out.t=T_NEQ; } else out.t=T_BANG; return out;
        case '&': 
            if (L->i<L->n && L->src[L->i]=='&'){ 
                L->i++; out.t=T_ANDAND; return out; 
            }
            fprintf(stderr,"Lex-Fehler: unerwartetes &\n"); exit(2);
        case '|': 
            if (L->i<L->n && L->src[L->i]=='|'){ 
                L->i++; out.t=T_OROR; return out; 
            }
            fprintf(stderr,"Lex-Fehler: unerwartetes |\n"); exit(2);
    }
    if (isdigit((unsigned char)c)){
        int v=c-'0';
        while (L->i<L->n && isdigit((unsigned char)L->src[L->i])) v=v*10+(L->src[L->i++]-'0');
        out.t=T_NUMBER; out.ival=v; return out;
    }
    if (is_ident_start((unsigned char)c)) {
        size_t k=0; out.s[k++]=c;
        while (L->i<L->n && is_ident_body((unsigned char)L->src[L->i]) && k+1<sizeof(out.s)) {
            out.s[k++]=L->src[L->i++];
        }
        out.s[k]=0;
        
        if (!strcmp(out.s,"func"))    { out.t=T_FUNC; return out; }
        if (!strcmp(out.s,"return"))  { out.t=T_RETURN; return out; }
        if (!strcmp(out.s,"if"))      { out.t=T_IF; return out; }
        if (!strcmp(out.s,"else"))    { out.t=T_ELSE; return out; }
        if (!strcmp(out.s,"while"))   { out.t=T_WHILE; return out; }
        if (!strcmp(out.s,"for"))     { out.t=T_FOR; return out; }
        if (!strcmp(out.s,"do"))      { out.t=T_DO; return out; }
        if (!strcmp(out.s,"switch"))  { out.t=T_SWITCH; return out; }
        if (!strcmp(out.s,"case"))    { out.t=T_CASE; return out; }
        if (!strcmp(out.s,"default")) { out.t=T_DEFAULT; return out; }
        if (!strcmp(out.s,"break"))   { out.t=T_BREAK; return out; }
        if (!strcmp(out.s,"continue")){ out.t=T_CONTINUE; return out; }
        if (!strcmp(out.s,"import"))  { out.t=T_IMPORT; return out; }
        if (!strcmp(out.s,"let"))     { out.t=T_LET; return out; }
        
        out.t=T_IDENT; 
        return out;
    }
    
    fprintf(stderr,"Lex-Fehler: %c\n",c); 
    exit(2);
}  // <-- NUR EINE schließende Klammer!
static void lex_init(Lex *L,const char*src){ L->src=src; L->i=0; L->n=strlen(src); L->cur.t=T_EOF; }
void advance(Lex *L){ L->cur=lex_next(L); }
int match(Lex*L,TokType t){ if(L->cur.t==t){ advance(L); return 1;} return 0; }

void expect(Lex*L,TokType t){
    if(!match(L,t)){
        fprintf(stderr,"Syntaxfehler: Erwartet %d (%s), bekam %d ('%s')\n",
            t, "???", L->cur.t, L->cur.s);  // <-- Mehr Debug-Info
        exit(2);
    }
}

// ---- Symboltabellen ----
uint8_t sym_get_slot(SymTab*T,const char*name){
    for(int i=0;i<T->n;i++) if(!strcmp(T->a[i].name,name)) return T->a[i].slot;
    strncpy(T->a[T->n].name,name,sizeof(T->a[0].name)-1);
    T->a[T->n].slot=(uint8_t)T->n; T->n++;
    return T->a[T->n-1].slot;
}

// ---- Funktionssymboltabelle ----
typedef struct { char name[64]; int addr; int nparams; } Func;
static Func funcs[256]; static int nfuncs=0;
static int find_func(const char*name){ for(int i=0;i<nfuncs;i++) if(!strcmp(funcs[i].name,name)) return i; return -1; }

// ---- Parser ----
void parse_stmt(P*p); void parse_expr(P*p);
static void parse_logical_or(P*p); static void parse_logical_and(P*p);
static void parse_equality(P*p); static void parse_rel(P*p);
static void parse_add(P*p); static void parse_mul(P*p);
static void parse_unary(P*p); static void parse_primary(P*p);

static void emit_pushi(P*p,int v){ emit_op(p->out,OP_PUSHI); emiti32(p->out,v); }

static void parse_program(P*p){
    advance(&p->L);
    while(p->L.cur.t!=T_EOF) parse_stmt(p);
    emit_op(p->out,OP_HALT);
}

static void parse_block(P*p){
    expect(&p->L,T_LBRACE);
    while(!match(&p->L,T_RBRACE)) parse_stmt(p);
}

void parse_stmt(P*p){
    switch(p->L.cur.t){
        case T_IF: advance(&p->L); parse_if(p); return;
        case T_WHILE: advance(&p->L); parse_while(p); return;
        case T_FOR: advance(&p->L); parse_for(p); return;
        case T_DO: advance(&p->L); parse_do_while(p); return;
        case T_SWITCH: advance(&p->L); parse_switch(p); return;
        case T_BREAK: advance(&p->L); parse_break(p); expect(&p->L, T_SEMI); return;
        case T_CONTINUE: advance(&p->L); parse_continue(p); expect(&p->L, T_SEMI); return;
        case T_IMPORT: advance(&p->L); parse_import(p); expect(&p->L, T_SEMI); return;
        case T_LET: advance(&p->L); parse_let_stmt(p); return;
    }

    if(match(&p->L,T_LBRACE)){ while(!match(&p->L,T_RBRACE)) parse_stmt(p); return; }

    // Funktionsdefinition
    if(p->L.cur.t==T_FUNC){
        advance(&p->L);
        if(p->L.cur.t != T_IDENT){
            fprintf(stderr,"Funktionsname erwartet.\n"); 
            exit(2);
        }
        char fname[64]; strncpy(fname,p->L.cur.s,sizeof(fname)); fname[63]=0;
        advance(&p->L);
        expect(&p->L,T_LPAREN);
        int params=0;
        while(p->L.cur.t==T_IDENT){
            sym_get_slot(&p->syms,p->L.cur.s);
            params++; advance(&p->L);
            if(!match(&p->L,T_COMMA)) break;
        }
        expect(&p->L,T_RPAREN);
        {
            int faddr=p->out->len;
            strncpy(funcs[nfuncs].name,fname,sizeof(funcs[nfuncs].name)-1);
            funcs[nfuncs].name[sizeof(funcs[nfuncs].name)-1]=0;
            funcs[nfuncs].addr=faddr; funcs[nfuncs].nparams=params; nfuncs++;
        }
        for (int i = params - 1; i >= 0; --i) {
            emit_op(p->out, OP_STOREL);
            emit8(p->out, (uint8_t)i);
        }
        parse_block(p);
        emit_op(p->out,OP_RET);
        match(&p->L, T_SEMI);
        return;
    }

    // return
    if(p->L.cur.t==T_RETURN){
        advance(&p->L);
        parse_expr(p);          // <-- DAS MUSS BLEIBEN!
        expect(&p->L,T_SEMI);
        emit_op(p->out,OP_RET);
        return;
    }

    // NEU: Identifier = Zuweisung oder Funktionsaufruf
    if (p->L.cur.t == T_IDENT) {
        parse_assignment_or_call_stmt(p);
        return;
    }

    // Standard-Ausdruck
    parse_expr(p); expect(&p->L,T_SEMI);
}

// ---- Ausdrücke ----
void parse_expr(P*p){ parse_logical_or(p); }

static void parse_logical_or(P*p){
    parse_logical_and(p);
    while(match(&p->L,T_OROR)){ parse_logical_and(p); emit_op(p->out,OP_OR); }
}

static void parse_logical_and(P*p){
    parse_equality(p);
    while(match(&p->L,T_ANDAND)){ parse_equality(p); emit_op(p->out,OP_AND); }
}

static void parse_equality(P*p){
    parse_rel(p);
    while(match(&p->L,T_EQEQ)){ parse_rel(p); emit_op(p->out,OP_EQ); }
    while(match(&p->L,T_NEQ)){ parse_rel(p); emit_op(p->out,OP_NE); }
}

static void parse_rel(P*p){
    parse_add(p);
    for(;;){
        if(match(&p->L,T_LT)){ parse_add(p); emit_op(p->out,OP_LT); }
        else if(match(&p->L,T_GT)){ parse_add(p); emit_op(p->out,OP_GT); }
        else if(match(&p->L,T_LE)){ parse_add(p); emit_op(p->out,OP_LE); }
        else if(match(&p->L,T_GE)){ parse_add(p); emit_op(p->out,OP_GE); }
        else break;
    }
}

static void parse_add(P*p){
    parse_mul(p);
    for(;;){
        if(match(&p->L,T_PLUS)){ parse_mul(p); emit_op(p->out,OP_ADD); }
        else if(match(&p->L,T_MINUS)){ parse_mul(p); emit_op(p->out,OP_SUB); }
        else break;
    }
}

static void parse_mul(P*p){
    parse_unary(p);
    for(;;){
        if(match(&p->L,T_STAR)){ parse_unary(p); emit_op(p->out,OP_MUL); }
        else if(match(&p->L,T_SLASH)){ parse_unary(p); emit_op(p->out,OP_DIV); }
        else break;
    }
}

static void parse_unary(P*p){
    if(match(&p->L,T_MINUS)){
        parse_unary(p);
        emit_op(p->out,OP_PUSHI); emiti32(p->out,-1);
        emit_op(p->out,OP_MUL);
        return;
    }
    if(match(&p->L,T_BANG)){
        parse_unary(p);
        emit_op(p->out,OP_NOT);
        return;
    }
    parse_primary(p);
}

// ---- Funktionsaufruf ----
void parse_call_and_emit(P*p,const char*name){
    int argc = 0;
    if(p->L.cur.t != T_RPAREN){
        for(;;){
            parse_expr(p);   // Argument
            argc++;
            if(match(&p->L,T_COMMA)) continue;
            break;
        }
    }
    expect(&p->L,T_RPAREN);

    int fid=find_func(name);
    if(fid>=0){
        if (argc != funcs[fid].nparams) {
            fprintf(stderr, "Arity-Fehler: %s erwartet %d Argumente, bekam %d.\n",
                    name, funcs[fid].nparams, argc);
            exit(2);
        }
        emit_op(p->out,OP_CALLUSER);
        emiti32(p->out,funcs[fid].addr);
        return;
    }

    if(!strcmp(name,"gpio_mode")){ emit_op(p->out,OP_CALL); emit8(p->out,0); return; }
    if(!strcmp(name,"gpio_write")){ emit_op(p->out,OP_CALL); emit8(p->out,1); return; }
    if(!strcmp(name,"sleep_ms")){ emit_op(p->out,OP_CALL); emit8(p->out,2); return; }
    if(!strcmp(name,"gpio_read")){ emit_op(p->out,OP_CALL); emit8(p->out,3); return; }
    if(!strcmp(name,"print_int")) {
        emit_op(p->out, OP_CALL);
        emit8(p->out, 4);   // neuer Index, muss mit Host übereinstimmen
        return;
    }


    fprintf(stderr,"Unbekannte Funktion: %s\n",name); exit(2);
}

// ---- Primary ----
static void parse_primary(P*p){
    Tok t=p->L.cur;
    if(t.t==T_NUMBER){
        advance(&p->L);
        emit_op(p->out,OP_PUSHI); emiti32(p->out,t.ival);
        return;
    }
    if(t.t==T_IDENT){
        char name[64]; strncpy(name,t.s,sizeof(name)); name[63]=0; advance(&p->L);
        if(match(&p->L,T_LPAREN)){ parse_call_and_emit(p,name); return; }
        { uint8_t slot=sym_get_slot(&p->syms,name);
          emit_op(p->out,OP_LOADL); emit8(p->out,slot); }
        return;
    }
    if(match(&p->L,T_LPAREN)){
        parse_expr(p); expect(&p->L,T_RPAREN); return;
    }
    fprintf(stderr,"Syntaxfehler in Ausdruck.\n"); exit(2);
}

// ---- I/O ----
static char* read_file(const char*path){
    FILE*f=fopen(path,"rb"); if(!f){perror("open"); exit(1);}
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    char*buf=(char*)malloc(n+1); fread(buf,1,n,f); buf[n]=0; fclose(f);
    return buf;
}
static void write_file(const char*path,const uint8_t*data,size_t n){
    FILE*f=fopen(path,"wb"); if(!f){perror("open out"); exit(1);}
    fwrite(data,1,n,f); fclose(f);
}

// ---- main ----
int main(int argc,char**argv){
    if(argc!=3){ fprintf(stderr,"Usage: %s in.mo out.bin\n",argv[0]); return 1; }
    char*src=read_file(argv[1]);
    Buf out; buf_init(&out);
    P p={0}; lex_init(&p.L,src); p.out=&out; p.syms.n=0; p.bc_sp=0;
    parse_program(&p);
    write_file(argv[2],out.data,out.len);
    free(out.data); free(src);
    return 0;
}