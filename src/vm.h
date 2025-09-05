#pragma once
#include <stdint.h>
#include <stddef.h>

typedef int32_t Val;

typedef enum {
    OP_HALT=0, OP_PUSHI, OP_LOADL, OP_STOREL,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_JMP, OP_JZ, OP_CALL,
    OP_LT, OP_EQ,
    OP_DUP, OP_DROP, OP_SWAP, OP_OVER,
    OP_GT, OP_GE, OP_LE, OP_NE,
    OP_NOT, OP_AND, OP_OR, OP_CALLUSER=24,
    OP_RET=25
} Op;

typedef struct {
  const uint8_t *code; 
  size_t code_len; 
  size_t ip;

  Val *stack; 
  size_t sp; 
  size_t stack_cap;

  Val *locals; 
  size_t locals_cap;

  void *hal;

  size_t callstack[256]; // Call-Stack für Rücksprungadressen
  size_t csp;            // Call-Stack-Pointer
} VM;


typedef enum { VM_OK=0, VM_TRAP=1 } VmRes;
VmRes vm_run(VM *vm);
