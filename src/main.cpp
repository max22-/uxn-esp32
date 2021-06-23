#include <Arduino.h>
#if defined(ESP32)
	#include <SPIFFS.h>
#endif
#if defined(ARDUINO_M5STACK_Core2)
	#include <M5Core2.h>
#endif

extern "C" {
	#include "uxn.h"
}

char* rom = (char*)"/spiffs/console.rom";
Uxn *u;

uint8_t reqdraw = 0;

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
		#warning Remove the comment below when the ppu will be integrated
		//putcolors(&ppu, &d->dat[0x8]);
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
		error("Memory", "Cannot allocate enough memory");
	if(!bootuxn(u))
		error("Boot", "Failed");
	if(!loaduxn(u, rom))
		error("Load", "Failed");

	portuxn(u, 0x0, (char*)"system", system_talk);
	portuxn(u, 0x1, (char*)"console", console_talk);
	portuxn(u, 0x2, (char*)"---", nil_talk);
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

	evaluxn(u, 0x100);

}

void loop() {
	int elapsed, start;
	start = micros();
	// do stuff
	elapsed = micros() - start;
	delayMicroseconds(clamp(16666 - elapsed, 0, 1000000));

}

