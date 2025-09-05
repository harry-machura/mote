#include "vm.h"
#include <string.h>
#include <stdio.h>

static int32_t rd_i32(const uint8_t *p){
    int32_t v;
    memcpy(&v, p, 4);
    return v;
}

// SAFE helper functions
static inline uint8_t SAFE_FETCH(VM *vm) {
    if (vm->ip >= vm->code_len) return 0;
    return vm->code[vm->ip++];
}

static inline void SAFE_PUSH(VM *vm, Val v) {
    if (vm->sp >= vm->stack_cap) return;
    vm->stack[vm->sp++] = v;
}

static inline Val SAFE_POP(VM *vm) {
    if (vm->sp == 0) return 0;
    return vm->stack[--vm->sp];
}

VmRes vm_run(VM *vm){

    struct HAL {
      void(*gpio_mode)(void*,int,int);
      void(*gpio_write)(void*,int,int);
      void(*sleep_ms)(void*,int);
      int (*gpio_read)(void*,int);
    } *H = (struct HAL*)vm->hal;

    while (vm->ip < vm->code_len){
        switch ((Op)SAFE_FETCH(vm)){
            case OP_HALT: 
                return VM_OK;

            case OP_PUSHI: {
                if (vm->ip + 4 > vm->code_len) return VM_TRAP;
                int32_t v = rd_i32(vm->code+vm->ip);
                vm->ip += 4;
                SAFE_PUSH(vm, v);
            } break;

            case OP_LOADL: {
                uint8_t idx = SAFE_FETCH(vm);
                if (idx >= vm->locals_cap) return VM_TRAP;
                SAFE_PUSH(vm, vm->locals[idx]);
            } break;

            case OP_STOREL: {
                uint8_t idx = SAFE_FETCH(vm);
                if (idx >= vm->locals_cap) return VM_TRAP;
                vm->locals[idx] = SAFE_POP(vm);
            } break;

            case OP_ADD: { Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm, a+b);} break;
            case OP_SUB: { Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm, a-b);} break;
            case OP_MUL: { Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm, a*b);} break;
            case OP_DIV: { Val b=SAFE_POP(vm), a=SAFE_POP(vm); if(b==0) return VM_TRAP; SAFE_PUSH(vm, a/b);} break;

            case OP_JMP: {
                if (vm->ip + 4 > vm->code_len) return VM_TRAP;
                int32_t off = rd_i32(vm->code+vm->ip);
                vm->ip += 4; vm->ip += off;
            } break;

            case OP_JZ: {
                if (vm->ip + 4 > vm->code_len) return VM_TRAP;
                int32_t off = rd_i32(vm->code+vm->ip);
                vm->ip += 4;
                if (SAFE_POP(vm) == 0) vm->ip += off;
            } break;

            case OP_LT: { Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm, a<b?1:0);} break;
            case OP_EQ: { Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm, a==b?1:0);} break;

            // Neue Stack Ops
            case OP_DUP: { Val v=SAFE_POP(vm); SAFE_PUSH(vm,v); SAFE_PUSH(vm,v);} break;
            case OP_DROP:{ (void)SAFE_POP(vm);} break;
            case OP_SWAP:{ Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm,b); SAFE_PUSH(vm,a);} break;
            case OP_OVER:{ Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm,a); SAFE_PUSH(vm,b); SAFE_PUSH(vm,a);} break;

            // Neue Vergleiche
            case OP_GT: { Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm, a>b?1:0);} break;
            case OP_GE: { Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm, a>=b?1:0);} break;
            case OP_LE: { Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm, a<=b?1:0);} break;
            case OP_NE: { Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm, a!=b?1:0);} break;

            // Logik
            case OP_NOT:{ Val a=SAFE_POP(vm); SAFE_PUSH(vm, a==0?1:0);} break;
            case OP_AND:{ Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm, (a!=0 && b!=0)?1:0);} break;
            case OP_OR: { Val b=SAFE_POP(vm), a=SAFE_POP(vm); SAFE_PUSH(vm, (a!=0 || b!=0)?1:0);} break;

            case OP_CALL: {
                uint8_t idx = SAFE_FETCH(vm);
                switch(idx){
                    case 0: { int pin=SAFE_POP(vm), mode=SAFE_POP(vm); H->gpio_mode(H,pin,mode); SAFE_PUSH(vm,0);} break;
                    case 1: { int pin=SAFE_POP(vm), val=SAFE_POP(vm);  H->gpio_write(H,pin,val); SAFE_PUSH(vm,0);} break;
                    case 2: { int ms=SAFE_POP(vm);                     H->sleep_ms(H,ms);        SAFE_PUSH(vm,0);} break;
                    case 3: { int pin=SAFE_POP(vm); int v=H->gpio_read(H,pin); SAFE_PUSH(vm,v);} break;
                    default: return VM_TRAP;
                }
            } break;

            case OP_CALLUSER: {
                int32_t addr = rd_i32(vm->code + vm->ip);
                vm->ip += 4;
                if (vm->csp >= 256) return VM_TRAP;   // Call-Stack-Overflow
                vm->callstack[vm->csp++] = vm->ip;    // Rücksprung speichern
                vm->ip = addr;                        // Springe zur Funktion
            } break;

            case OP_RET: {
                if (vm->csp == 0) return VM_OK;       // Main beendet
                vm->ip = vm->callstack[--vm->csp];    // Rücksprung laden
            } break;

            default:
                return VM_TRAP;
        }
    }
    return VM_TRAP;
}
