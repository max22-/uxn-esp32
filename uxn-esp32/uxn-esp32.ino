#include <SPIFFS.h>
#include <TFT_eSPI.h>

extern "C" {
  #include "src/uxn.h"
  #include "src/devices/system.h"
  #include "src/devices/file.h"
  #include "src/devices/datetime.h"
  #include "src/devices/screen.h"
  #include "src/devices/controller.h"
  #include "src/devices/mouse.h"
}

/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

char *rom_path = "/spiffs/basic.rom";
TFT_eSPI tft = TFT_eSPI();
uint16_t *line;

static Uxn u;
static Device *devscreen, *devctrl, *devmouse;

/* Compilation error when we put a newline after the type declaration O__O */
static void error(char *msg, const char *err)
{
	fprintf(stderr, "Error %s: %s\n", msg, err);
	while(true)
    delay(1000);
}

void
system_deo_special(Device *d, Uint8 port)
{
	if(port > 0x7 && port < 0xe)
		screen_palette(uxn_screen, &d->dat[0x8]);
}

static void console_deo(Device *d, Uint8 port)
{
	FILE *fd = port == 0x8 ? stdout : port == 0x9 ? stderr
												  : 0;
	if(fd) {
		fputc(d->dat[port], fd);
		fflush(fd);
	}
}

static Uint8 nil_dei(Device *d, Uint8 port)
{
	return d->dat[port];
}

static void nil_deo(Device *d, Uint8 port)
{
	(void)d;
	(void)port;
}

static int console_input(Uxn *u, char c)
{
	Device *d = &u->dev[1];
	d->dat[0x2] = c;
	return uxn_eval(u, GETVECTOR(d));
}

int
uxn_interrupt(void)
{
	return 1;
}

static int start(Uxn *u)
{
	if(!uxn_boot(u, (Uint8 *)calloc(0x10000, sizeof(Uint8))))
		error("Boot", "Failed");
	/* system   */ uxn_port(u, 0x0, system_dei, system_deo);
	/* console  */ uxn_port(u, 0x1, nil_dei, console_deo);
	/* screen   */ devscreen = uxn_port(u, 0x2, screen_dei, screen_deo);
	/* empty    */ uxn_port(u, 0x3, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x4, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x5, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x6, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x7, nil_dei, nil_deo);
	/* control  */ devctrl = uxn_port(u, 0x8, nil_dei, nil_deo);
	/* mouse    */ devmouse = uxn_port(u, 0x9, nil_dei, nil_deo);
	/* file0    */ uxn_port(u, 0xa, file_dei, file_deo);
	/* file1    */ uxn_port(u, 0xb, file_dei, file_deo);
	/* datetime */ uxn_port(u, 0xc, datetime_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xd, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xe, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xf, nil_dei, nil_deo);
	return 1;
}

void setup()
{
	int i;
  Serial.begin(115200);
  Serial.println("boot");
  SPIFFS.begin();

  tft.begin();
  tft.initDMA();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  Serial.println("Starting uxn");
	if(!start(&u))
		error("Start", "Failed");
  Serial.println("Loading rom");
	if(!load_rom(&u, rom_path))
		error("Load", "Failed");
	fprintf(stderr, "Loaded %s\n", rom_path);

  Serial.println("allocating uxn_screen struct");
  uxn_screen = (UxnScreen*)malloc(sizeof(UxnScreen));
  if(!uxn_screen)
    error("UxnScreen", "malloc");
  uxn_screen->bg.pixels=NULL;
  uxn_screen->fg.pixels=NULL;
  Serial.println("screen_resize");
  if(!screen_resize(uxn_screen, tft.width(), tft.height()))
    error("Screen", "Failed to allocate memory");

  Serial.println("Calling uxneval(&u, PAGE_PROGRAM)");
	if(!uxn_eval(&u, PAGE_PROGRAM))
		error("Init", "Failed");
  line = (Uint16*)heap_caps_malloc(tft.width() * sizeof(Uint16), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
  if(!line)
    error("screen", "failed to allocate memory for scanline");
  /*
	for(i = 2; i < argc; i++) {
		char *p = argv[i];
		while(*p) console_input(&u, *p++);
		console_input(&u, '\n');
	}
  */
}


void loop()
{/*
  if
  	Device *d = &u->dev[0];
	while(!d->dat[0xf]) {
		int c = fgetc(stdin);
		if(c != EOF)
			console_input(u, (Uint8)c);
	}*/
  
  uxn_eval(&u, GETVECTOR(devscreen));

  uint32_t w = uxn_screen->width, h = uxn_screen->height;
  uint8_t *fg = uxn_screen->fg.pixels, *bg=uxn_screen->bg.pixels;
  Uint16 palette[16], palette_mono[2] = {TFT_BLACK, TFT_WHITE};
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
