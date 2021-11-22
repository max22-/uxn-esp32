#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPIFFS.h>
extern "C" {
  #include "uxn.h"
}

static const char *rom = "/spiffs/console.rom";

static TFT_eSPI tft = TFT_eSPI();
static Device *devsystem, *devconsole;

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

static void
inspect(Stack *s, const char *name)
{
	Uint8 x, y;
	fprintf(stderr, "\n%s\n", name);
	for(y = 0; y < 0x04; ++y) {
		for(x = 0; x < 0x08; ++x) {
			Uint8 p = y * 0x08 + x;
			fprintf(stderr,
				p == s->ptr ? "[%02x]" : " %02x ",
				s->dat[p]);
		}
		fprintf(stderr, "\n");
	}
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
	case 0xe:
		inspect(&d->u->wst, "Working-stack");
		inspect(&d->u->rst, "Return-stack");
		break;
	}
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

void setup() {
  Uxn *u;
  Serial.begin(115200);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  SPIFFS.begin();

  u = (Uxn*)malloc(sizeof(Uxn));
  if(u == NULL)
    error("Malloc", "Failed");
  
  if(!uxn_boot(u)) 
    error("Boot", "Failed");

  /* system   */ devsystem = uxn_port(u, 0x0, system_dei, system_deo);
	/* console  */ devconsole = uxn_port(u, 0x1, nil_dei, console_deo);
	/* empty    */ uxn_port(u, 0x2, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x3, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x4, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x5, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x6, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x7, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x8, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x9, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xa, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xb, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xc, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xd, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xe, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xf, nil_dei, nil_deo);

  if(!load(u, rom))
    error("Load", "Failed");

  if(!uxn_eval(u, PAGE_PROGRAM))
    error("Init", "Failed");
}

void loop() {
  // put your main code here, to run repeatedly:
}