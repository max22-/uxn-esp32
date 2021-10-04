#include <time.h>
#include <unistd.h>

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Seeed_Arduino_FS.h>

extern "C" {
#include "uxn.h"
#include "devices/ppu.h"

static Device *devsystem, *devconsole;

void
console_printf(const char* format, ...)
{
	va_list args;
	va_start(args, format);   

	char buf[50];
    vsnprintf(buf, 50, format, args);
    Serial.print(buf);
	va_end(args);   
}

static const char *errors[] = {"underflow", "overflow", "division by zero"};

int
uxn_halt(Uxn *u, Uint8 error, char *name, int id)
{
	console_printf("Halted: %s %s#%04x, at 0x%04x\n", name, errors[error - 1], id, u->ram.ptr);
	return 0;
}
} // extern "C"

static int
error(char *msg, const char *err)
{
	console_printf("Error %s: %s\n", msg, err);
	return 0;
}

static void
inspect(Stack *s, char *name)
{
	Uint8 x, y;
	console_printf("\n%s\n", name);
	for(y = 0; y < 0x04; ++y) {
		for(x = 0; x < 0x08; ++x) {
			Uint8 p = y * 0x08 + x;
			console_printf(
				p == s->ptr ? "[%02x]" : " %02x ",
				s->dat[p]);
		}
		console_printf("\n");
	}
}

static int
system_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(!w) { /* read */
		switch(b0) {
		case 0x2: d->dat[0x2] = d->u->wst.ptr; break;
		case 0x3: d->dat[0x3] = d->u->rst.ptr; break;
		}
	} else { /* write */
		switch(b0) {
		case 0x2: d->u->wst.ptr = d->dat[0x2]; break;
		case 0x3: d->u->rst.ptr = d->dat[0x3]; break;
		case 0xe:
			inspect(&d->u->wst, "Working-stack");
			inspect(&d->u->rst, "Return-stack");
			break;
		case 0xf: return 0;
		}
	}
	return 1;
}

static int
console_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(w && b0 > 0x7)
#ifdef ARDUINO
		console_printf("%c", d->dat[b0]);
#else
		write(b0 - 0x7, (char *)&d->dat[b0], 1);
#endif
	return 1;
}

static int
file_talk(Device *d, Uint8 b0, Uint8 w)
{
	Uint8 read = b0 == 0xd;
	if(w && (read || b0 == 0xf)) {
		char *name = (char *)&d->mem[peek16(d->dat, 0x8)];
		Uint16 result = 0, length = peek16(d->dat, 0xa);
		long offset = (peek16(d->dat, 0x4) << 16) + peek16(d->dat, 0x6);
		Uint16 addr = peek16(d->dat, b0 - 1);
		FILE *f = fopen(name, read ? "rb" : (offset ? "ab" : "wb"));
		if(f) {
			if(fseek(f, offset, SEEK_SET) != -1)
				result = read ? fread(&d->mem[addr], 1, length, f) : fwrite(&d->mem[addr], 1, length, f);
			fclose(f);
		}
		poke16(d->dat, 0x2, result);
	}
	return 1;
}

static int
datetime_talk(Device *d, Uint8 b0, Uint8 w)
{
	time_t seconds = time(NULL);
	struct tm *t = localtime(&seconds);
	t->tm_year += 1900;
	poke16(d->dat, 0x0, t->tm_year);
	d->dat[0x2] = t->tm_mon;
	d->dat[0x3] = t->tm_mday;
	d->dat[0x4] = t->tm_hour;
	d->dat[0x5] = t->tm_min;
	d->dat[0x6] = t->tm_sec;
	d->dat[0x7] = t->tm_wday;
	poke16(d->dat, 0x08, t->tm_yday);
	d->dat[0xa] = t->tm_isdst;
	(void)b0;
	(void)w;
	return 1;
}

static int
nil_talk(Device *d, Uint8 b0, Uint8 w)
{
	(void)d;
	(void)b0;
	(void)w;
	return 1;
}

void
uxn_run(Uxn *u)
{
	Uint16 vec = PAGE_PROGRAM;
	uxn_eval(u, vec);
	while((!u->dev[0].dat[0xf]) && (read(0, &devconsole->dat[0x2], 1) > 0)) {
		vec = peek16(devconsole->dat, 0);
		if(!vec) vec = u->ram.ptr; /* continue after last BRK */
		uxn_eval(u, vec);
	}
}

int
uxn_load(Uxn *u, char *filepath)
{
	File f;
	if (!(f = SD.open(filepath))) {
        console_printf("Error loading \"%s\"\n", filepath);
		return 0;
	}
	f.read(u->ram.dat + PAGE_PROGRAM, sizeof(u->ram.dat) - PAGE_PROGRAM);
	f.close();
	console_printf("Loaded %s\n", filepath);
	return 1;
}

#include "monitor.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

Uxn u;
Ppu ppu;

char rom[] = "/uxn/bin/hello.rom";

static int
uxn_setup(Uxn* u, char* rom)
{
	if(!uxn_boot(u))
		return error("Boot", "Failed");
	if(!uxn_load(u, rom))
		return error("Load", "Failed");

	/* system   */ devsystem = uxn_port(u, 0x0, system_talk);
	/* console  */ devconsole = uxn_port(u, 0x1, console_talk);
	/* empty    */ uxn_port(u, 0x2, nil_talk);
	/* empty    */ uxn_port(u, 0x3, nil_talk);
	/* empty    */ uxn_port(u, 0x4, nil_talk);
	/* empty    */ uxn_port(u, 0x5, nil_talk);
	/* empty    */ uxn_port(u, 0x6, nil_talk);
	/* empty    */ uxn_port(u, 0x7, nil_talk);
	/* empty    */ uxn_port(u, 0x8, nil_talk);
	/* empty    */ uxn_port(u, 0x9, nil_talk);
	/* file     */ uxn_port(u, 0xa, file_talk);
	/* datetime */ uxn_port(u, 0xb, datetime_talk);
	/* empty    */ uxn_port(u, 0xc, nil_talk);
	/* empty    */ uxn_port(u, 0xd, nil_talk);
	/* empty    */ uxn_port(u, 0xe, nil_talk);
	/* empty    */ uxn_port(u, 0xf, nil_talk);

	return 1;
}

void
setup()
{
    Serial.begin(115200);
	while (!Serial);

    if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI, 4000000UL)) {
		console_printf("SDcard initialization failed!");
  	}

	tft.init();
	tft.setRotation(3);
	tft.fillScreen(TFT_BLACK);
	tft.setCursor(0, 0);
	tft.setTextColor(TFT_GREEN);
	spr.setColorDepth(4);
/*
	if (spr.createSprite(8 * hor, 8 * ver) == NULL) {
		error("tTFT_eSPI", "Cannot create sprite");
		quit();
	}
*/
    if (!uxn_setup(&u, rom)) {
        return;
    }

    mon_init();
	//uxn_run(&u);
}

void
loop()
{
    char c;
    if (Serial.available() > 0) {
        c = Serial.read();
        mon_onechar(&u, c);
    }
}