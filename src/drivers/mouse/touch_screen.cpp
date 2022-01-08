#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
extern "C" {
#include <uxn.h>
#include <devices/mouse.h>
}

#ifdef USE_TOUCH_SCREEN

extern TFT_eSPI tft;

int 
devmouse_init() {
    uint16_t calData[5] = CAL_DATA;
    tft.setTouch(calData);
    return 1;
}

int
devmouse_handle(Device *d) {
  Uint16 x, y;
  static Uint8 old_pressed = 0;
  Uint8 pressed = 0;

  /* The pressed/released state is not really stable (at least on the screen i have tested) */
  if (tft.getTouch(&x, &y)) {
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