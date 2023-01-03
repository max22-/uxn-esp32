#include <FFat.h>
#include <TFT_eSPI.h>
#include "touchscreen.h"

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

char *rom_path = "/ffat/potato.rom";

static Uxn u;
static Device *devscreen, *devctrl, *devmouse;

/* Compilation error when we put a newline after the type declaration O__O */
void error(char *msg, const char *err)
{
	fprintf(stderr, "Error %s: %s\n", msg, err);
  tft.printf("Error %s: %s\n", msg, err);
  
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
  touchscreen_init();
  if(!FFat.begin(true))
    error("FFat", "Failed to initialize file system");

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
  touchscreen_touch(devmouse);
  uxn_eval(&u, GETVECTOR(devscreen));
  touchscreen_redraw();
}
