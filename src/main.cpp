#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#if defined(ESP32)
#include <SPIFFS.h>
#include "arduino-drivers/esp32/audio/audio.h"
#endif

extern "C" {
#include "uxn.h"
#include "devices/ppu.h"
#include "devices/apu.h"
}

#include "utility.h"

// Config

static char *rom = (char *)"/spiffs/screen.rom";
const int hor = 40, ver = 30;

// *******

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

#define POLYPHONY 4

static Uxn *u;
static Ppu *ppu;
static Apu *apu[POLYPHONY];
static Device *devsystem, *devscreen, *devmouse, *devaudio0, *devconsole;

static Uint8 reqdraw = 0;

static Uint8 uxn_font[][8] = {	// there is already a variable named "font" in TFT_eSPI.h
	{0x00, 0x7c, 0x82, 0x82, 0x82, 0x82, 0x82, 0x7c},
	{0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
	{0x00, 0x7c, 0x82, 0x02, 0x7c, 0x80, 0x80, 0xfe},
	{0x00, 0x7c, 0x82, 0x02, 0x1c, 0x02, 0x82, 0x7c},
	{0x00, 0x0c, 0x14, 0x24, 0x44, 0x84, 0xfe, 0x04},
	{0x00, 0xfe, 0x80, 0x80, 0x7c, 0x02, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x80, 0xfc, 0x82, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x02, 0x1e, 0x02, 0x02, 0x02},
	{0x00, 0x7c, 0x82, 0x82, 0x7c, 0x82, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x82, 0x7e, 0x02, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x02, 0x7e, 0x82, 0x82, 0x7e},
	{0x00, 0xfc, 0x82, 0x82, 0xfc, 0x82, 0x82, 0xfc},
	{0x00, 0x7c, 0x82, 0x80, 0x80, 0x80, 0x82, 0x7c},
	{0x00, 0xfc, 0x82, 0x82, 0x82, 0x82, 0x82, 0xfc},
	{0x00, 0x7c, 0x82, 0x80, 0xf0, 0x80, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x80, 0xf0, 0x80, 0x80, 0x80}};

int
clamp(int val, int min, int max)
{
	return (val >= min) ? (val <= max) ? val : max : min;
}

static void 
quit()
{
	tft.fillScreen(TFT_BLACK);
	while(true)
		delay(1000);
}

static void
error(char *msg, const char *err)
{
	fprintf(stderr, "%s: %s\n", msg, err);
	quit();
}

static void
audio_callback(Sint16 *stream, size_t bytes)
{
	int i;
	memset(stream, 0, bytes);
	for(i = 0; i < POLYPHONY; ++i)
		apu_render(apu[i], stream, stream + bytes / sizeof(Sint16));
}

static void
inspect(Ppu *p, Uint8 *stack, Uint8 wptr, Uint8 rptr, Uint8 *memory)
{
	Uint8 i, x, y, b;
	for(i = 0; i < 0x20; ++i) { /* stack */
		x = ((i % 8) * 3 + 1) * 8, y = (i / 8 + 1) * 8, b = stack[i];
		ppu_1bpp(p, 1, x, y, uxn_font[(b >> 4) & 0xf], 1 + (wptr == i) * 0x7, 0, 0);
		ppu_1bpp(p, 1, x + 8, y, uxn_font[b & 0xf], 1 + (wptr == i) * 0x7, 0, 0);
	}
	/* return pointer */
	ppu_1bpp(p, 1, 0x8, y + 0x10, uxn_font[(rptr >> 4) & 0xf], 0x2, 0, 0);
	ppu_1bpp(p, 1, 0x10, y + 0x10, uxn_font[rptr & 0xf], 0x2, 0, 0);
	for(i = 0; i < 0x20; ++i) { /* memory */
		x = ((i % 8) * 3 + 1) * 8, y = 0x38 + (i / 8 + 1) * 8, b = memory[i];
		ppu_1bpp(p, 1, x, y, uxn_font[(b >> 4) & 0xf], 3, 0, 0);
		ppu_1bpp(p, 1, x + 8, y, uxn_font[b & 0xf], 3, 0, 0);
	}
	for(x = 0; x < 0x10; ++x) { /* guides */
		ppu_pixel(p, 1, x, p->height / 2, 2);
		ppu_pixel(p, 1, p->width - x, p->height / 2, 2);
		ppu_pixel(p, 1, p->width / 2, p->height - x, 2);
		ppu_pixel(p, 1, p->width / 2, x, 2);
		ppu_pixel(p, 1, p->width / 2 - 0x10 / 2 + x, p->height / 2, 2);
		ppu_pixel(p, 1, p->width / 2, p->height / 2 - 0x10 / 2 + x, 2);
	}
}

void
redraw(Uxn* u)
{
	if(devsystem->dat[0xe])
		inspect(ppu, u->wst.dat, u->wst.ptr, u->rst.ptr, u->ram.dat);
	spr.pushSprite(0, 0);
	reqdraw = 0;
}

static int
uxn_init(void)
{
	if(!ppu_init(ppu, hor, ver))
		error("PPU", "Init failure");
	#warning uncomment this when audio will be implemented
	//initaudio(audio_callback);
	ppu->pixels = (Uint8*)spr.getPointer();
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
	} else if(b0 > 0x7 && b0 < 0xe) {
		uint8_t r[16], g[16], b[16];
		uint16_t pal[16];
		int i;
		for(i = 0; i < 4; ++i) {
			r[i] = ((d->dat[0x8 + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
			g[i] = ((d->dat[0xa + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
			b[i] = ((d->dat[0xc + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
			pal[i] = spr.color565(r[i], g[i], b[i]);
		}
		for(i = 4; i < 16; ++i) {
			r[i] = r[i / 4];
			g[i] = g[i / 4];
			b[i] = b[i / 4];
			pal[i] = spr.color565(r[i], g[i], b[i]);
		}
		spr.createPalette(pal);
		reqdraw = 1;
	} else if(b0 == 0xf)
		d->u->ram.ptr = 0x0000;
}

static void
console_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(w && b0 > 0x7)
		write(b0 - 0x7, (char *)&d->dat[b0], 1);
}

static void
screen_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(w && b0 == 0xe) {
		Uint16 x = mempeek16(d->dat, 0x8);
		Uint16 y = mempeek16(d->dat, 0xa);
		Uint8 layer = d->dat[0xe] >> 4 & 0x1;
		ppu_pixel(ppu, layer, x, y, d->dat[0xe] & 0x3);
		reqdraw = 1;
	} else if(w && b0 == 0xf) {
		Uint16 x = mempeek16(d->dat, 0x8);
		Uint16 y = mempeek16(d->dat, 0xa);
		Uint8 layer = d->dat[0xf] >> 0x6 & 0x1;
		Uint8 *addr = &d->mem[mempeek16(d->dat, 0xc)];
		if(d->dat[0xf] >> 0x7 & 0x1)
			ppu_2bpp(ppu, layer, x, y, addr, d->dat[0xf] & 0xf, d->dat[0xf] >> 0x4 & 0x1, d->dat[0xf] >> 0x5 & 0x1);
		else
			ppu_1bpp(ppu, layer, x, y, addr, d->dat[0xf] & 0xf, d->dat[0xf] >> 0x4 & 0x1, d->dat[0xf] >> 0x5 & 0x1);
		reqdraw = 1;
	}
}

static void
audio_talk(Device *d, Uint8 b0, Uint8 w)
{
	Apu *c = apu[d - devaudio0];
	#warning Replace the following line by something else if we can't open the I2S sound output ?
	//if(!audio_id) return;
	if(!w) {
		if(b0 == 0x2)
			mempoke16(d->dat, 0x2, c->i);
		else if(b0 == 0x4)
			d->dat[0x4] = apu_get_vu(c);
	} else if(b0 == 0xf) {
		audio_lock();
		c->len = mempeek16(d->dat, 0xa);
		c->addr = &d->mem[mempeek16(d->dat, 0xc)];
		c->volume[0] = d->dat[0xe] >> 4;
		c->volume[1] = d->dat[0xe] & 0xf;
		c->repeat = !(d->dat[0xf] & 0x80);
		apu_start(c, mempeek16(d->dat, 0x8), d->dat[0xf] & 0x7f);
		audio_unlock();
	}
}

/*

void
domouse()
{
	static bool oldPressed = false;
	bool pressed;
	static Uint16 oldX = 0, oldY = 0;
	Uint16 x = 0, y = 0;
	Uint8 flag = 0;

#if defined(ARDUINO_M5STACK_Core2)
	pressed = M5.Touch.ispressed();
	if(pressed) {
		TouchPoint_t c = M5.Touch.getPressPoint();
		if(c.x >= 0 && c.y >= 0) {
			x = c.x;
			y = c.y;
		} else {
			x = oldX;
			y = oldY;
		}
		printf("(%d, %d)\n", x, y);
	}
#else
#warning domouse() not implemented on this board
#endif

	if(pressed) { // Mouse moves
		x = clamp(x, 0, ppu->hor * 8 - 1);
		y = clamp(y, 0, ppu->ver * 8 - 1);
		mempoke16(devmouse->dat, 0x2, x);
		mempoke16(devmouse->dat, 0x4, y);
	}

	if(pressed != oldPressed) { // Mouse clicks
		flag = 0x01;
		// flag = 0x10; for the right button, but we don't have it on a touchscreen :(
		if(pressed)
			devmouse->dat[6] |= flag;
		else
			devmouse->dat[6] &= (~flag);
	}
	if(pressed != oldPressed || x != oldX || y != oldY)
		evaluxn(u, mempeek16(devmouse->dat, 0));

	oldPressed = pressed;
	oldX = x;
	oldY = y;
}

*/

static const char *errors[] = {"underflow", "overflow", "division by zero"};

int
uxn_halt(Uxn *u, Uint8 error, char *name, int id)
{
	fprintf(stderr, "Halted: %s %s#%04x, at 0x%04x\n", name, errors[error - 1], id, u->ram.ptr);
	u->ram.ptr = 0;
	return 0;
}

static void
run(Uxn* u)
{
	uxn_eval(u, 0x0100);
	redraw(u);
	int elapsed, start;
	start = micros();

	while(true) {
		/*
		domouse();
		*/

		uxn_eval(u, mempeek16(devscreen->dat, 0));

		if(reqdraw)
			redraw(u);

		elapsed = micros() - start;
		delayMicroseconds(clamp(16666 - elapsed, 0, 1000000));
	}
}

static int
load(Uxn *u, char *filepath)
{
	FILE *f;
	if(!(f = fopen(filepath, "rb")))
		return 0;
	fread(u->ram.dat + PAGE_PROGRAM, sizeof(u->ram.dat) - PAGE_PROGRAM, 1, f);
	fprintf(stderr, "Loaded %s\n", filepath);
	return 1;
}

void
setup()
{
	Serial.begin(115200);
	tft.init();
	tft.setRotation(3);
	tft.fillScreen(TFT_BLACK);
	tft.setCursor(0, 0);
	tft.setTextColor(TFT_GREEN);
	spr.setColorDepth(4);
	if(spr.createSprite(8 * hor, 8 * ver) == nullptr) {
		error("tTFT_eSPI", "Cannot create sprite");
		quit();
	}

#if defined(ESP32)
	SPIFFS.begin();
#endif

	if((u = (Uxn *)malloc(sizeof(Uxn))) == nullptr)
		error("Memory", "Cannot allocate enough memory for uxn");
	if((ppu = (Ppu *)malloc(sizeof(Ppu))) == nullptr)
		error("Memory", "Cannot allocate enough memory for the ppu");
	for(int i = 0; i < POLYPHONY; i++) {
		if((apu[i] = (Apu *)malloc(sizeof(Apu))) == nullptr)
			error("Memory", "Cannot allocate enough memory for the apu");
		memset(apu[i], 0, sizeof(apu));
	}

	if(!uxn_boot(u))
		error("Boot", "Failed to start uxn.");
	if(!load(u, rom))
		error("Load", "Failed");
	if(!uxn_init())
		error("Init", "failed");

	devsystem = uxn_port(u, 0x0, (char *)"system", system_talk);
	devconsole = uxn_port(u, 0x1, (char *)"console", console_talk);
	devscreen = uxn_port(u, 0x2, (char *)"screen", screen_talk);
	devaudio0 = uxn_port(u, 0x3, (char *)"audio0", audio_talk);
	uxn_port(u, 0x4, (char *)"audio1", audio_talk);
	uxn_port(u, 0x5, (char *)"audio2", audio_talk);
	uxn_port(u, 0x6, (char *)"audio3", audio_talk);
	uxn_port(u, 0x7, (char *)"---", nil_talk);
	uxn_port(u, 0x8, (char *)"---", nil_talk);
	devmouse = uxn_port(u, 0x9, (char *)"mouse", nil_talk);
	uxn_port(u, 0xa, (char *)"---", nil_talk);
	uxn_port(u, 0xb, (char *)"---", nil_talk);
	uxn_port(u, 0xc, (char *)"---", nil_talk);
	uxn_port(u, 0xd, (char *)"---", nil_talk);
	uxn_port(u, 0xe, (char *)"---", nil_talk);
	uxn_port(u, 0xf, (char *)"---", nil_talk);

	/* Write screen size to dev/screen */
	mempoke16(devscreen->dat, 2, ppu->width);
	mempoke16(devscreen->dat, 4, ppu->height);
	tft.println("Starting Uxn in 2 seconds");
	delay(2000);
}

void
loop()
{
	run(u);
	quit();
}