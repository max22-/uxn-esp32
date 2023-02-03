extern "C" {
  #include "src/uxn.h"
}
void touchscreen_init();
void touchscreen_calibrate();
void touchscreen_touch(Device *devmouse);
void touchscreen_redraw();

extern TFT_eSPI tft;