#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
extern "C" {
#include <uxn.h>
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
devmouse_handle(Uxn *u) {
  Device *devmouse = &u->dev[0x9];
  Uint16 x, y;
  Uint8 eval_flag = 0;
  static Uint8 pressed = 0;

  /* The pressed/released state is not really stable (at least on the screen i have tested) */
  if (tft.getTouch(&x, &y)) {
    poke16(devmouse->dat, 0x2, x);
    poke16(devmouse->dat, 0x4, y);
    eval_flag = 1;
    pressed = 1;
  } else {
    eval_flag = pressed == 1;
    pressed = 0;
  }
  devmouse->dat[0x6] = pressed;
  if(eval_flag)
    uxn_eval(u, devmouse->vector);

  return 1;
}

#endif