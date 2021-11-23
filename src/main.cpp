#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPIFFS.h>
extern "C" {
  #include "uxn.h"
  #include "devices/ppu.h"
  #include "devices/file.h"
}

/********** Config ***********/
#define USE_WIFI
const char* ntp_server = "pool.ntp.org";
const long gmt_offset_sec = 3600;
const int daylight_offset_sec = 3600;
static const char *rom = "/spiffs/datetime.rom";
/*****************************/

#ifdef USE_WIFI
#include "WiFi.h"
#include "wifi_credentials.h"
#include "time.h"
#endif

static TFT_eSPI tft = TFT_eSPI();
static TFT_eSprite screen_sprite(&tft);
static Ppu ppu;
static Device *devsystem, *devconsole, *devscreen;

static void
error(const char *msg, const char *err)
{
  fprintf(stderr, "Error %s: %s\n", msg, err);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_GREEN);
  tft.printf("Error %s: %s\n", msg, err);
	while(1)
    delay(1000);
}

void
set_palette(Uint8 *addr)
{
	int i;
	Uint16 palette[16];
	for(i = 0; i < 4; ++i) {
		Uint8
			r = (*(addr + i / 2) >> (!(i % 2) << 2)) & 0x0f,
			g = (*(addr + 2 + i / 2) >> (!(i % 2) << 2)) & 0x0f,
			b = (*(addr + 4 + i / 2) >> (!(i % 2) << 2)) & 0x0f;
		palette[i] = (r << 12) | (g << 7) | (b << 1);
		screen_sprite.setPaletteColor(i, palette[i]);
	}
	for(i = 4; i < 16; ++i) {
		palette[i] = palette[i / 4];
		screen_sprite.setPaletteColor(i, palette[i]);
	}
	ppu.reqdraw = 1;
}

static void
redraw(Uxn *u)
{
	if(devsystem->dat[0xe])
		ppu_debug(&ppu, u->wst.dat, u->wst.ptr, u->rst.ptr, u->ram.dat);
	screen_sprite.pushSprite(0, 0);
	ppu.reqdraw = 0;
}

static Uint8
system_dei(Device *d, Uint8 port)
{
	switch(port) {
	case 0x2: return d->u->wst.ptr;
	case 0x3: return d->u->rst.ptr;
	default: return d->dat[port];
	}
}

static void
system_deo(Device *d, Uint8 port)
{
	switch(port) {
	case 0x2: d->u->wst.ptr = d->dat[port]; break;
	case 0x3: d->u->rst.ptr = d->dat[port]; break;
	}
	if(port > 0x7 && port < 0xe)
		set_palette(&d->dat[0x8]);
}

static void
console_deo(Device *d, Uint8 port)
{
	if(port == 0x1)
		d->vector = peek16(d->dat, 0x0);
	if(port > 0x7)
		write(port - 0x7, (char *)&d->dat[port], 1);
}

static Uint8
screen_dei(Device *d, Uint8 port)
{
	switch(port) {
	case 0x2: return ppu.width >> 8;
	case 0x3: return ppu.width;
	case 0x4: return ppu.height >> 8;
	case 0x5: return ppu.height;
	default: return d->dat[port];
	}
}

static void
screen_deo(Device *d, Uint8 port)
{
	switch(port) {
	case 0x1: d->vector = peek16(d->dat, 0x0); break;
	case 0x5:
		/*
		if(!FIXED_SIZE) set_size(peek16(d->dat, 0x2), peek16(d->dat, 0x4), 1);
		Not supported for little devices :)
		*/
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
}

static void
file_deo(Device *d, Uint8 port)
{
	switch(port) {
	case 0x1: d->vector = peek16(d->dat, 0x0); break;
	case 0x9: poke16(d->dat, 0x2, file_init(&d->mem[peek16(d->dat, 0x8)])); break;
	case 0xd: poke16(d->dat, 0x2, file_read(&d->mem[peek16(d->dat, 0xc)], peek16(d->dat, 0xa))); break;
	case 0xf: poke16(d->dat, 0x2, file_write(&d->mem[peek16(d->dat, 0xe)], peek16(d->dat, 0xa), d->dat[0x7])); break;
	case 0x5: poke16(d->dat, 0x2, file_stat(&d->mem[peek16(d->dat, 0x4)], peek16(d->dat, 0xa))); break;
	case 0x6: poke16(d->dat, 0x2, file_delete()); break;
	}
}

static Uint8
datetime_dei(Device *d, Uint8 port)
{
	time_t seconds = time(NULL);
	struct tm zt = {0};
	struct tm *t = localtime(&seconds);
	if(t == NULL)
		t = &zt;
	switch(port) {
	case 0x0: return (t->tm_year + 1900) >> 8;
	case 0x1: return (t->tm_year + 1900);
	case 0x2: return t->tm_mon;
	case 0x3: return t->tm_mday;
	case 0x4: return t->tm_hour;
	case 0x5: return t->tm_min;
	case 0x6: return t->tm_sec;
	case 0x7: return t->tm_wday;
	case 0x8: return t->tm_yday >> 8;
	case 0x9: return t->tm_yday;
	case 0xa: return t->tm_isdst;
	default: return d->dat[port];
	}
}

static Uint8
nil_dei(Device *d, Uint8 port)
{
	return d->dat[port];
}

static void
nil_deo(Device *d, Uint8 port)
{
	if(port == 0x1) d->vector = peek16(d->dat, 0x0);
}

static const char *errors[] = {"underflow", "overflow", "division by zero"};

int
uxn_halt(Uxn *u, Uint8 error, char *name, int id)
{
	fprintf(stderr, "Halted: %s %s#%04x, at 0x%04x\n", name, errors[error - 1], id, u->ram.ptr);
	return 0;
}

static int
load(Uxn *u, const char *filepath)
{
	FILE *f;
	int r;
	if(!(f = fopen(filepath, "rb"))) return 0;
	r = fread(u->ram.dat + PAGE_PROGRAM, 1, sizeof(u->ram.dat) - PAGE_PROGRAM, f);
	fclose(f);
	if(r < 1) return 0;
	fprintf(stderr, "Loaded %s\n", filepath);
	return 1;
}

static void
run(Uxn *u)
{
  char c;
  unsigned long ts, elapsed;
  redraw(u);
  while(!devsystem->dat[0xf]) {
	ts = micros();
	if(Serial.available() > 0) {
		Serial.readBytes(&c, 1);
		devconsole->dat[0x2] = c;
		if(!uxn_eval(u, devconsole->vector))
			error("Console", "eval failed");
	}
	uxn_eval(u, devscreen->vector);
	if(ppu.reqdraw || devsystem->dat[0xe])
	  redraw(u);
	elapsed = micros() - ts;
	if(elapsed < 16666)
		delayMicroseconds(16666 - elapsed);

  }
}

void setup() {
  Uxn *u;
  Uint8 *pixels;
  Serial.begin(115200);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(0, 0);

#ifdef USE_WIFI
  tft.printf("Connecting to \"%s\"", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while(WiFi.status() != WL_CONNECTED) {
	  delay(500);
	  tft.print(".");
  }
  WiFi.setAutoReconnect(true);
  Serial.println("Connected.");
  configTime(gmt_offset_sec, daylight_offset_sec, ntp_server);
#endif

  screen_sprite.setColorDepth(4);
  pixels = (Uint8*)screen_sprite.createSprite(tft.width(), tft.height());
  if(pixels == NULL)
	error("TFT_eSPI", "createSprite failed");
  ppu_init(&ppu, tft.width(), tft.height(), pixels);
  
  SPIFFS.begin();

  u = (Uxn*)malloc(sizeof(Uxn));
  if(u == NULL)
    error("Malloc", "Failed");
  
  if(!uxn_boot(u)) 
    error("Boot", "Failed");

    /* system   */ devsystem = uxn_port(u, 0x0, system_dei, system_deo);
	/* console  */ devconsole = uxn_port(u, 0x1, nil_dei, console_deo);
	/* screen   */ devscreen = uxn_port(u, 0x2, screen_dei, screen_deo);
	/* empty    */ uxn_port(u, 0x3, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x4, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x5, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x6, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x7, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x8, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x9, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xa, nil_dei, file_deo);
	/* datetime */ uxn_port(u, 0xb, datetime_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xc, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xd, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xe, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xf, nil_dei, nil_deo);

  if(!load(u, rom))
    error("Load", "Failed");

  if(!uxn_eval(u, PAGE_PROGRAM))
    error("Init", "Failed");

  run(u);
}

void loop() {
}