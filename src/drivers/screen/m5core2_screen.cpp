#include "config.h"
#ifdef USE_M5CORE2_SCREEN

#include <M5Core2.h>
extern "C" {
    #include <uxn.h>
    #include <devices/screen.h>
}

extern void error(const char *msg, const char *err);

Uint16 *line;

int
devscreen_init() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(TFT_GREEN);
  M5.Lcd.setCursor(0, 0);
  line = (Uint16*)heap_caps_malloc(M5.Lcd.width() * sizeof(Uint16), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
  if(line == NULL)
    error("devscreen_init", "not enough memory");
  screen_resize(&uxn_screen, M5.Lcd.width(), M5.Lcd.height());
  if(uxn_screen.pixels == NULL)
    error("devscreen_init", "not enough memory");
  return 1;
}

int 
devscreen_redraw() {
  Uint32 x, y, w = uxn_screen.width, h = uxn_screen.height;
  Uint16 palette[16], c;
  Uint8 *pixels = uxn_screen.pixels, p, p1, p2;
  int i;

  unsigned long timestamp = micros();

	for(i = 0; i < 16; i++) {
    c = uxn_screen.palette[(i >> 2) ? (i >> 2) : (i & 3)];
    //palette[i] = c >> 8 | c << 8; /* We swap bytes for the tft screen */
    palette[i] = c;
  }

  M5.Lcd.startWrite();
  M5.Lcd.setWindow(0, 0, w, h);

  /* TODO: write  more efficient loop */
  /* but it is not the bottleneck (the SPI port IS) */
  for(y=0; y< h; y++) {
    for(x = 0; x < w; x+=2) {
      p = pixels[(y*w+x)/2];
      p1 = p >> 4;
      p2 = p & 0xf;
      line[x] = palette[p1];
      line[x+1] = palette[p2];
    }
    /*Serial.printf("Busy ? %s\n", tft.dmaBusy() ? "true" : "false");*/
    M5.Lcd.writePixels(line, w);
  }
  M5.Lcd.endWrite();
  uxn_screen.changed = 0;
   Serial.printf("\n%ld us\n", micros() - timestamp); 
  return 1; 
}

#endif