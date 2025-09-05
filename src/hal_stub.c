#include "vm.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static void hal_gpio_mode(void*ctx,int pin,int mode){
  (void)ctx; printf("[HAL] gpio_mode pin=%d mode=%d\n", pin, mode);
}
static void hal_gpio_write(void*ctx,int pin,int val){
  (void)ctx; printf("[HAL] gpio_write pin=%d val=%d\n", pin, val);
}
static void hal_sleep_ms(void*ctx,int ms){
  (void)ctx;
#ifdef _WIN32
  Sleep(ms);
#else
  usleep(ms*1000);
#endif
}

static int hal_gpio_read(void*ctx,int pin){
  (void)ctx; (void)pin;
  static int counter=0, state=1;
  if (++counter % 200 == 0) state = !state;   // wechselt periodisch
  printf("[HAL] gpio_read pin=%d -> %d\n", pin, state);
  return state;
}

void* mote_bind_hal(){
  static struct {
    void(*gpio_mode)(void*,int,int);
    void(*gpio_write)(void*,int,int);
    void(*sleep_ms)(void*,int);
    int (*gpio_read)(void*,int);              // NEU
  } vtbl = { hal_gpio_mode, hal_gpio_write, hal_sleep_ms, hal_gpio_read };
  return &vtbl;
}
