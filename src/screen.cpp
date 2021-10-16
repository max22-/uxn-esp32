/**
 * An Uxn screen device for the Wio Terminal
 */
#include <Arduino.h>
#include <TFT_eSPI.h>

extern "C" {
#include "uxn.h"
#include "devices/ppu.h"
}

#include "screen.h"
#include "monitor.h"

#define WIDTH 40 * 8
#define HEIGHT 30 * 8

#define POLYPHONY 4
#define BENCH 0

class Wio_eSprite : public TFT_eSprite {
  public:
	Wio_eSprite(TFT_eSPI *tft): TFT_eSprite(tft)
	{}

    void* getPointer(void)
	{
		if (!_created) return nullptr;
		return _img8_1;
	}

	void createPalette(uint16_t *palette = nullptr, uint8_t colors = 16)
	{}
};

static Ppu ppu;
static Device* devscreen;
static TFT_eSPI tft = TFT_eSPI();
static Wio_eSprite spr = Wio_eSprite(&tft);

//static uint8_t screen_pixels[WIDTH*HEIGHT];

static Uint8 screen_font[][8] = {
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

static void
draw_inspect(Ppu *p, Uint8 *stack, Uint8 wptr, Uint8 rptr, Uint8 *memory)
{
	Uint8 i, x, y, b;
	for(i = 0; i < 0x20; ++i) {
		x = ((i % 8) * 3 + 1) * 8, y = (i / 8 + 1) * 8, b = stack[i];
		/* working stack */
		ppu_1bpp(p, 1, x, y, screen_font[(b >> 4) & 0xf], 1 + (wptr == i) * 0x7, 0, 0);
		ppu_1bpp(p, 1, x + 8, y, screen_font[b & 0xf], 1 + (wptr == i) * 0x7, 0, 0);
		y = 0x28 + (i / 8 + 1) * 8;
		b = memory[i];
		/* return stack */
		ppu_1bpp(p, 1, x, y, screen_font[(b >> 4) & 0xf], 3, 0, 0);
		ppu_1bpp(p, 1, x + 8, y, screen_font[b & 0xf], 3, 0, 0);
	}
	/* return pointer */
	ppu_1bpp(p, 1, 0x8, y + 0x10, screen_font[(rptr >> 4) & 0xf], 0x2, 0, 0);
	ppu_1bpp(p, 1, 0x10, y + 0x10, screen_font[rptr & 0xf], 0x2, 0, 0);
	/* guides */
	for(x = 0; x < 0x10; ++x) {
		ppu_write(p, 1, x, p->height / 2, 2);
		ppu_write(p, 1, p->width - x, p->height / 2, 2);
		ppu_write(p, 1, p->width / 2, p->height - x, 2);
		ppu_write(p, 1, p->width / 2, x, 2);
		ppu_write(p, 1, p->width / 2 - 0x10 / 2 + x, p->height / 2, 2);
		ppu_write(p, 1, p->width / 2, p->height / 2 - 0x10 / 2 + x, 2);
	}
}

void
screen_redraw(Uxn *u)
{
	if (u->dev[0].dat[0xe])
		draw_inspect(&ppu, u->wst.dat, u->wst.ptr, u->rst.ptr, u->ram.dat);
	spr.pushSprite(0, 0);
	ppu.reqdraw = 0;
}

static int
screen_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(!w) switch(b0) {
		case 0x2: d->dat[0x2] = ppu.width >> 8; break;
		case 0x3: d->dat[0x3] = ppu.width; break;
		case 0x4: d->dat[0x4] = ppu.height >> 8; break;
		case 0x5: d->dat[0x5] = ppu.height; break;
		}
	else
		switch(b0) {
		case 0x1: d->vector = peek16(d->dat, 0x0); break;
		case 0x5:
			// Fixed sized => set_size ignored
			break;
		case 0xe: {
			Uint16 x = peek16(d->dat, 0x8);
			Uint16 y = peek16(d->dat, 0xa);
			Uint8 layer = d->dat[0xe] & 0x40;
			ppu_write(&ppu, !!layer, x, y, d->dat[0xe] & 0x3);
			if(d->dat[0x6] & 0x01) poke16(d->dat, 0x8, x + 1); /* auto x+1 */
			if(d->dat[0x6] & 0x02) poke16(d->dat, 0xa, y + 1); /* auto y+1 */
			break;
		}
		case 0xf: {
			Uint16 x = peek16(d->dat, 0x8);
			Uint16 y = peek16(d->dat, 0xa);
			Uint8 layer = d->dat[0xf] & 0x40;
			Uint8 *addr = &d->mem[peek16(d->dat, 0xc)];
			if(d->dat[0xf] & 0x80) {
				ppu_2bpp(&ppu, !!layer, x, y, addr, d->dat[0xf] & 0xf, d->dat[0xf] & 0x10, d->dat[0xf] & 0x20);
				if(d->dat[0x6] & 0x04) poke16(d->dat, 0xc, peek16(d->dat, 0xc) + 16); /* auto addr+16 */
			} else {
				ppu_1bpp(&ppu, !!layer, x, y, addr, d->dat[0xf] & 0xf, d->dat[0xf] & 0x10, d->dat[0xf] & 0x20);
				if(d->dat[0x6] & 0x04) poke16(d->dat, 0xc, peek16(d->dat, 0xc) + 8); /* auto addr+8 */
			}
			if(d->dat[0x6] & 0x01) poke16(d->dat, 0x8, x + 8); /* auto x+8 */
			if(d->dat[0x6] & 0x02) poke16(d->dat, 0xa, y + 8); /* auto y+8 */
			break;
		}
		}
	return 1;
}

void
screen_init(Uxn* u)
{
	extern int error(char *msg, const char *err);
	
	tft.init();
	tft.setRotation(3);
	tft.fillScreen(TFT_BLACK);
	tft.setCursor(0, 0);
	tft.setTextColor(TFT_GREEN);
	spr.setColorDepth(4);

	if (spr.createSprite(WIDTH, HEIGHT) == NULL) {
		error("tTFT_eSPI", "Cannot create sprite");
		return;
	}

	ppu.width = WIDTH;
	ppu.height = HEIGHT;
	//ppu.pixels = (Uint8*) malloc(WIDTH * HEIGHT * sizeof(Uint8) / 2);
	ppu.pixels = (Uint8*)spr.getPointer();

	//ppu_set_size(&ppu, WIDTH, HEIGHT);

	devscreen = uxn_port(u, 0x2, screen_talk);

	poke16(devscreen->dat, 2, ppu.width);
	poke16(devscreen->dat, 4, ppu.height);

	Serial.println("Init complete.");
	tft.println("Starting Uxn in 2 seconds");
	delay(2000);
}

void
screen_loop(Uxn* u)
{
	uxn_eval(u, devscreen->vector);
	if (ppu.reqdraw || u->dev[0].dat[0xe])
		screen_redraw(u);
}