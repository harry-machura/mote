#include "vm.h"
#include <stdio.h>
#include <stdlib.h>

extern void* mote_bind_hal();

int main(int argc, char**argv){
  if (argc!=2){ fprintf(stderr,"Usage: %s program.bin\n", argv[0]); return 1; }
  FILE*f=fopen(argv[1],"rb"); if(!f){perror("open"); return 1;}
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  uint8_t*code=(uint8_t*)malloc(n); fread(code,1,n,f); fclose(f);

  Val stack[256]={0};
  Val locals[8]={0};

  VM vm = {
    .code=code, .code_len=(size_t)n, .ip=0,
    .stack=stack, .sp=0, .stack_cap=256,
    .locals=locals, .locals_cap=8,
    .hal=mote_bind_hal()
  };

  VmRes r = vm_run(&vm);
  printf("VM exit: %s, sp=%zu\n", r==VM_OK?"OK":"TRAP", vm.sp);
  free(code);
  return r==VM_OK?0:2;
}
