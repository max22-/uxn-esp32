#include <M5Core2.h>

extern "C" {
  #include "uxnemu.h"
}

char* rom = (char*)"piano.rom";

void setup() {
  Serial.begin(115200);
  M5.begin();
  M5.Lcd.println("Hello, world");
  delay(10000);
  M5.Lcd.println("Booting uxn");
  M5.Lcd.printf("rom = \"%s\"\n", rom);
  uxn_main(rom);
}

void loop() {
  
}