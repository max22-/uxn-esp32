#ifndef ARDUINO

#include <stdio.h>
#include <time.h>
#include "uxn.h"

/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#pragma mark - Core

int
error(char *msg, const char *err)
{
	printf("Error %s: %s\n", msg, err);
	return 0;
}

void
printstack(Stack *s)
{
	Uint8 x, y;
	printf("\n\n");
	for(y = 0; y < 0x08; ++y) {
		for(x = 0; x < 0x08; ++x) {
			Uint8 p = y * 0x08 + x;
			printf(p == s->ptr ? "[%02x]" : " %02x ", s->dat[p]);
		}
		printf("\n");
	}
}

#pragma mark - Devices

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
file_talk(Device *d, Uint8 b0, Uint8 w)
{
	Uint8 read = b0 == 0xd;
	if(w && (read || b0 == 0xf)) {
		char *name = (char *)&d->mem[mempeek16(d->dat, 0x8)];
		Uint16 result = 0, length = mempeek16(d->dat, 0xa);
		Uint16 offset = mempeek16(d->dat, 0x4);
		Uint16 addr = mempeek16(d->dat, b0 - 1);
		FILE *f = fopen(name, read ? "r" : (offset ? "a" : "w"));
		if(f) {
			printf("%s %04x %s %s: ", read ? "Loading" : "Saving", addr, read ? "from" : "to", name);
			if(fseek(f, offset, SEEK_SET) != -1)
				result = read ? fread(&d->mem[addr], 1, length, f) : fwrite(&d->mem[addr], 1, length, f);
			printf("%04x bytes\n", result);
			fclose(f);
		}
		mempoke16(d->dat, 0x2, result);
	}
}

void
datetime_talk(Device *d, Uint8 b0, Uint8 w)
{
	time_t seconds = time(NULL);
	struct tm *t = localtime(&seconds);
	t->tm_year += 1900;
	mempoke16(d->dat, 0x0, t->tm_year);
	d->dat[0x2] = t->tm_mon;
	d->dat[0x3] = t->tm_mday;
	d->dat[0x4] = t->tm_hour;
	d->dat[0x5] = t->tm_min;
	d->dat[0x6] = t->tm_sec;
	d->dat[0x7] = t->tm_wday;
	mempoke16(d->dat, 0x08, t->tm_yday);
	d->dat[0xa] = t->tm_isdst;
	(void)b0;
	(void)w;
}

void
nil_talk(Device *d, Uint8 b0, Uint8 w)
{
	(void)d;
	(void)b0;
	(void)w;
}

#pragma mark - Generics

int
start(Uxn *u)
{
	if(!evaluxn(u, PAGE_PROGRAM))
		return error("Reset", "Failed");
	return 1;
}

int
main(int argc, char **argv)
{
	Uxn u;

	if(argc < 2)
		return error("Input", "Missing");
	if(!bootuxn(&u))
		return error("Boot", "Failed");
	if(!loaduxn(&u, argv[1]))
		return error("Load", "Failed");

	portuxn(&u, 0x0, "empty", nil_talk);
	portuxn(&u, 0x1, "console", console_talk);
	portuxn(&u, 0x2, "empty", nil_talk);
	portuxn(&u, 0x3, "empty", nil_talk);
	portuxn(&u, 0x4, "empty", nil_talk);
	portuxn(&u, 0x5, "empty", nil_talk);
	portuxn(&u, 0x6, "empty", nil_talk);
	portuxn(&u, 0x7, "empty", nil_talk);
	portuxn(&u, 0x8, "empty", nil_talk);
	portuxn(&u, 0x9, "empty", nil_talk);
	portuxn(&u, 0xa, "file", file_talk);
	portuxn(&u, 0xb, "datetime", datetime_talk);
	portuxn(&u, 0xc, "empty", nil_talk);
	portuxn(&u, 0xd, "empty", nil_talk);
	portuxn(&u, 0xe, "empty", nil_talk);
	portuxn(&u, 0xf, "empty", nil_talk);

	start(&u);

	if(argc > 2)
		printstack(&u.wst);
	return 0;
}

#endif