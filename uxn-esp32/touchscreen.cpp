#include <TFT_eSPI.h>
#include <FFat.h>
extern "C" {
  #include "src/uxn.h"
  #include "src/devices/screen.h"
  #include "src/devices/mouse.h"
}

TFT_eSPI tft = TFT_eSPI();
static uint16_t *line;

extern void error(char *msg, const char *err);

void touchscreen_init()
{
  tft.begin();
  tft.initDMA();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  line = (uint16_t*)heap_caps_malloc(tft.width() * sizeof(Uint16), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
  if(!line)
    error("screen", "failed to allocate memory for scanline");
  uint16_t calData[5];
  tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
}

void touchscreen_calibrate(uint16_t *calData)
{
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(20, 5);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Touch corners as indicated");
  tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
}

void touchscreen_touch(Device *devmouse)
{
  uint16_t x, y;
  static bool old_state = false;
  bool pressed = tft.getTouch(&x, &y);
  if(pressed) mouse_pos(devmouse, x, y);
  if(pressed != old_state) {
    if(pressed) mouse_down(devmouse, 0x1);
    else mouse_up(devmouse, 0x0);
  }
}

void touchscreen_redraw()
{
  uint32_t w = uxn_screen->width, h = uxn_screen->height;
  uint8_t *fg = uxn_screen->fg.pixels, *bg=uxn_screen->bg.pixels;
  uint16_t palette[16], palette_mono[2] = {TFT_BLACK, TFT_WHITE};
  //uint8_t mono = uxn_screen->mono;
  uint8_t mono = 0;
  uint32_t c32;
  uint16_t c16;

  for(int i = 0; i < 16; i++) {
    c32 = uxn_screen->palette[(i >> 2) ? (i >> 2) : (i & 3)];
    c16 = tft.color24to16(c32 & 0Xffffff);
    palette[i] = c16 >> 8 | c16 << 8; /* We swap bytes for the tft screen */
  }

  tft.startWrite();
  tft.setAddrWindow(0, 0, w, h);

  for(int y=0; y< h; y++) {
    for(int x = 0; x < w; x++) {
      int pixnum = y*w + x;
      int idx = pixnum / 4;
      uint8_t shift = (pixnum%4)*2;
      #define GETPIXEL(i) (((i)>>shift) & 0x3)
      uint8_t fg_pixel = GETPIXEL(fg[idx]), bg_pixel = GETPIXEL(bg[idx]);

      if(mono) {
        if(fg_pixel)
          line[x] = palette_mono[fg_pixel];
        else line[x] = palette_mono[bg_pixel&0x1];
      }
      else 
        line[x] = palette[fg_pixel << 2 | bg_pixel];
    }
    /*Serial.printf("Busy ? %s\n", tft.dmaBusy() ? "true" : "false");*/
    tft.pushPixelsDMA(line, w);
  }
  tft.endWrite();

  uxn_screen->fg.changed=0;
  uxn_screen->bg.changed=0; 
}
