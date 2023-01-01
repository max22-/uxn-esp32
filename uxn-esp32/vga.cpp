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
static uint8_t palette_mono[2];

void IRAM_ATTR drawScanline(void * arg, uint8_t * dest, int scanLine)
{
  uint8_t *fg = uxn_screen->fg.pixels, *bg=uxn_screen->bg.pixels;
  bool mono = uxn_screen->mono;

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
      uint8_t fg_pixel = GETPIXEL(fg[idx]), bg_pixel = GETPIXEL(bg[idx]);
      if(mono) {
        VGA_PIXELINROW(dest, x) = palette_mono[0];
        //VGA_PIXELINROW(dest, x) = palette_mono[fg_pixel ? fg_pixel : bg_pixel & 0x1];
      }
      else
        VGA_PIXELINROW(dest, x) = palette[fg_pixel << 2 | bg_pixel];
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
  palette_mono[0] = DisplayController.createRawPixel(RGB222(0, 0, 0));
  palette_mono[1] = DisplayController.createRawPixel(RGB222(1, 1, 1));
}

void vga_sync()
{
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}