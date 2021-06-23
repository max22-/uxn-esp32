#include <Arduino.h>
#if defined(ESP32)
	#include <SPIFFS.h>
#endif
#if defined(ARDUINO_M5STACK_Core2)
	#include <M5Core2.h>
#endif

extern "C" {
	#include "uxn.h"
	#include "devices/ppu.h"
}

#include "utility.h"

// Config

static char* rom = (char*)"/spiffs/screen.rom";
static const Uint8 hor = 40, ver = 30;

// *******


static Uxn *u;
static Ppu *ppu;
static Uint16 *screenbuf;
static Device *devscreen;

static Uint8 reqdraw = 0;

int
clamp(int val, int min, int max)
{
	return (val >= min) ? (val <= max) ? val : max : min;
}

void
error(const char* msg, const char* err)
{
	printf("Error %s: %s\n", msg, err);
	for(;;)
		delay(1000);
}

void
redraw()
{
	#warning redraw() not implemented

	
	Uint16 w = ppu->width, h = ppu->height;
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			int l = x+y*w;
			Uint8 
				bgcolor = ppu->bg.pixels[l],
				fgcolor = ppu->fg.pixels[l];
			screenbuf[l] = fgcolor == 0 ? ppu->bg.colors[bgcolor] : ppu->fg.colors[fgcolor];
		}
	}


	#if defined(ARDUINO_M5STACK_Core2)
	M5.Lcd.drawBitmap(0, 0, ppu->width, ppu->height, screenbuf);
	#else
	#warning redraw not implemented for this board
	#endif
	reqdraw = 0;
}

int
inituxn(void)
{
	if(!initppu(ppu, hor, ver))
		error("PPU", "Init failure");
	if((screenbuf = (Uint16*)ext_malloc(sizeof(Uint16)*ppu->width*ppu->height)) == nullptr)
		error("PPU", "Can't allocate memory");
	return 1;
}

void
nil_talk(Device *d, Uint8 b0, Uint8 w)
{
	(void)d;
	(void)b0;
	(void)w;
}

void
system_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(!w) {
		d->dat[0x2] = d->u->wst.ptr;
		d->dat[0x3] = d->u->rst.ptr;
	} else {
		putcolors(ppu, &d->dat[0x8]);
		reqdraw = 1;
	}
	(void)b0;
}

void
console_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(!w) return;
	switch(b0) {
	case 0x8: printf("%c", d->dat[0x8]); break;
	case 0x9: printf("0x%02x", d->dat[0x9]); break;
	case 0xb: printf("0x%04x", mempeek16(d->dat, 0xa)); break;
	case 0xd: printf("%s", &d->mem[mempeek16(d->dat, 0xc)]); break;
	}
	fflush(stdout);
}

void
screen_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(w && b0 == 0xe) {
		Uint16 x = mempeek16(d->dat, 0x8);
		Uint16 y = mempeek16(d->dat, 0xa);
		Uint8 *addr = &d->mem[mempeek16(d->dat, 0xc)];
		Layer *layer = d->dat[0xe] >> 4 & 0x1 ? &ppu->fg : &ppu->bg;
		Uint8 mode = d->dat[0xe] >> 5;
		if(!mode)
			putpixel(ppu, layer, x, y, d->dat[0xe] & 0x3);
		else if(mode-- & 0x1)
			puticn(ppu, layer, x, y, addr, d->dat[0xe] & 0xf, mode & 0x2, mode & 0x4);
		else
			putchr(ppu, layer, x, y, addr, d->dat[0xe] & 0xf, mode & 0x2, mode & 0x4);
		reqdraw = 1;
	}
}

void
uxnmain()
{
	int elapsed, start;
		start = micros();
		
		if(reqdraw)
			redraw();

		elapsed = micros() - start;
		delayMicroseconds(clamp(16666 - elapsed, 0, 1000000));
}

void setup() {
	#if defined(ARDUINO_M5STACK_Core2)
		M5.begin(true, false, true);
	#else
		Serial.begin(115200);
	#endif
	#if defined(ESP32)
		SPIFFS.begin();
	#endif
	if((u = (Uxn*)malloc(sizeof(Uxn))) == nullptr)
		error("Memory", "Cannot allocate enough memory for uxn");
	if((ppu = (Ppu*)malloc(sizeof(Ppu))) == nullptr)
		error("Memory", "Cannot allocate enough memory for the ppu");
	if(!bootuxn(u))
		error("Boot", "Failed");
	if(!loaduxn(u, rom))
		error("Load", "Failed");
	if(!inituxn())
		error("Init", "failed");

	portuxn(u, 0x0, (char*)"system", system_talk);
	portuxn(u, 0x1, (char*)"console", console_talk);
	devscreen = portuxn(u, 0x2, (char*)"screen", screen_talk);
	portuxn(u, 0x3, (char*)"---", nil_talk);
	portuxn(u, 0x4, (char*)"---", nil_talk);
	portuxn(u, 0x5, (char*)"---", nil_talk);
	portuxn(u, 0x6, (char*)"---", nil_talk);
	portuxn(u, 0x7, (char*)"---", nil_talk);
	portuxn(u, 0x8, (char*)"---", nil_talk);
	portuxn(u, 0x9, (char*)"---", nil_talk);
	portuxn(u, 0xa, (char*)"---", nil_talk);
	portuxn(u, 0xb, (char*)"---", nil_talk);
	portuxn(u, 0xc, (char*)"---", nil_talk);
	portuxn(u, 0xd, (char*)"---", nil_talk);
	portuxn(u, 0xe, (char*)"---", nil_talk);
	portuxn(u, 0xf, (char*)"---", nil_talk);

	/* Write screen size to dev/screen */
	mempoke16(devscreen->dat, 2, ppu->hor * 8);
	mempoke16(devscreen->dat, 4, ppu->ver * 8);

	evaluxn(u, 0x100);
	redraw();

}

void loop() {
	uxnmain();
}

