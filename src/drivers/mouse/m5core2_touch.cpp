#include "config.h"
#ifdef USE_M5CORE2_TOUCH

#include <M5Core2.h>
extern "C" {
#include <uxn.h>
#include <devices/mouse.h>
}

int 
devmouse_init() {
    return 1;
}

int
devmouse_handle(Device *d) {
  Uint16 x, y;
  static Uint8 old_pressed = 0;
  Uint8 pressed = 0;

  if (M5.Lcd.getTouch(&x, &y)) {
    mouse_pos(d, x, y);
    pressed = 1;
  }

  if(pressed != old_pressed) {
    if(pressed)
      mouse_down(d, 0x1);
    else
      mouse_up(d, 0x1);
  }

  old_pressed = pressed;

  return 1;
}

#endif