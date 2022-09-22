#include <Arduino.h>
#include "fabgl.h"

fabgl::VGADirectController DisplayController;

constexpr int        scanlinesPerCallback = 2;
static TaskHandle_t  mainTaskHandle;

void IRAM_ATTR drawScanline(void * arg, uint8_t * dest, int scanLine)
{
  auto fgcolor = DisplayController.createRawPixel(RGB222(3, 0, 0)); // red
  auto bgcolor = DisplayController.createRawPixel(RGB222(0, 0, 2)); // blue
  auto width  = DisplayController.getScreenWidth();
  auto height = DisplayController.getScreenHeight();

  for (int i = 0; i < scanlinesPerCallback; ++i) {
    memset(dest, bgcolor, width);
    ++scanLine;
    dest += width;
  }
  if (scanLine == height) {
    // signal end of screen
    vTaskNotifyGiveFromISR(mainTaskHandle, NULL);
  }
}


void setup()
{
  mainTaskHandle = xTaskGetCurrentTaskHandle();
  DisplayController.begin();
  DisplayController.setScanlinesPerCallBack(scanlinesPerCallback);
  DisplayController.setDrawScanlineCallback(drawScanline);
  DisplayController.setResolution(VGA_640x480_60Hz);
}


void loop()
{
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}
