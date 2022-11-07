#include "fabgl.h"
extern "C" {
  #include "src/uxn.h"
  #include "src/devices/screen.h"
}

fabgl::VGADirectController DisplayController;

constexpr int        scanlinesPerCallback = 2;
static TaskHandle_t  mainTaskHandle;

static int width, height;
static uint8_t palette[16];

void IRAM_ATTR drawScanline(void * arg, uint8_t * dest, int scanLine)
{
  uint8_t *fg = uxn_screen->fg.pixels, *bg=uxn_screen->bg.pixels;

  if(scanLine == 2) { // beginning of new frame
    for(int i = 0; i < 16; i++) {
      Uint32 rgba = uxn_screen->palette[(i >> 2) ? (i >> 2) : (i & 3)];
      Uint8 r = (rgba >> 16) & 0xff, g = (rgba >> 8) & 0xff, b = rgba & 0xff;
      palette[i] = DisplayController.createRawPixel(RGB222(RGB888(r, g, b)));
    }
  }
  
  for (int i = 0; i < scanlinesPerCallback; ++i) {
    for(int x=0; x < width; x++) {
      int pixnum = scanLine*width + x;
      int idx = pixnum / 4;
      uint8_t shift = (pixnum%4)*2;
      #define GETPIXEL(x) (((x)>>shift) & 0x3)
      VGA_PIXELINROW(dest, x) = palette[GETPIXEL(fg[idx]) << 2 | GETPIXEL(bg[idx])];
    }
    ++scanLine;
    dest += width;
  }
  if (scanLine == height) {
    // signal end of screen
    vTaskNotifyGiveFromISR(mainTaskHandle, NULL);
  }
}

void vga_begin()
{
  mainTaskHandle = xTaskGetCurrentTaskHandle();
  DisplayController.begin();
  DisplayController.setScanlinesPerCallBack(scanlinesPerCallback);
  DisplayController.setDrawScanlineCallback(drawScanline);
  DisplayController.setResolution(QVGA_320x240_60Hz);
  width = DisplayController.getScreenWidth();
  height = DisplayController.getScreenHeight();
}

void vga_sync()
{
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}