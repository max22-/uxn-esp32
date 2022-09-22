#include "vga.h"

void setup()
{
  vga_begin();
}


void loop()
{
  vga_sync();
}
