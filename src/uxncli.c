#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
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

static Device *devsystem, *devconsole;

static int
error(char *msg, const char *err)
{
	fprintf(stderr, "Error %s: %s\n", msg, err);
	return 0;
}

#pragma mark - Devices

static void
system_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(!w) {
		d->dat[0x2] = d->u->wst.ptr;
		d->dat[0x3] = d->u->rst.ptr;
	} else if(b0 == 0xe) {
		Uint8 x, y;
		fprintf(stderr, "\n\n");
		for(y = 0; y < 0x08; ++y) {
			for(x = 0; x < 0x08; ++x) {
				Uint8 p = y * 0x08 + x;
				fprintf(stderr,
					p == d->u->wst.ptr ? "[%02x]" : " %02x ",
					d->u->wst.dat[p]);
			}
			fprintf(stderr, "\n");
		}
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
			fprintf(stderr, "%s %s %s #%04x, ", read ? "Loading" : "Saving", name, read ? "to" : "from", addr);
			if(fseek(f, offset, SEEK_SET) != -1)
				result = read ? fread(&d->mem[addr], 1, length, f) : fwrite(&d->mem[addr], 1, length, f);
			fprintf(stderr, "%04x bytes\n", result);
			fclose(f);
		}
		mempoke16(d->dat, 0x2, result);
	}
}

static void
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

static void
nil_talk(Device *d, Uint8 b0, Uint8 w)
{
	(void)d;
	(void)b0;
	(void)w;
}

#pragma mark - Generics

static const char *errors[] = {"underflow", "overflow", "division by zero"};

int
uxn_halt(Uxn *u, Uint8 error, char *name, int id)
{
	fprintf(stderr, "Halted: %s %s#%04x, at 0x%04x\n", name, errors[error - 1], id, u->ram.ptr);
	u->ram.ptr = 0;
	return 0;
}

static void
run(Uxn *u)
{
	if(!uxn_eval(u, PAGE_PROGRAM))
		error("Reset", "Failed");
	else if(mempeek16(devconsole->dat, 0))
		while(read(0, &devconsole->dat[0x2], 1) > 0)
			uxn_eval(u, mempeek16(devconsole->dat, 0));
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

int
main(int argc, char **argv)
{
	Uxn u;

	if(argc < 2)
		return error("Input", "Missing");
	if(!uxn_boot(&u))
		return error("Boot", "Failed");
	if(!load(&u, argv[1]))
		return error("Load", "Failed");

	devsystem = uxn_port(&u, 0x0, "system", system_talk);
	devconsole = uxn_port(&u, 0x1, "console", console_talk);
	uxn_port(&u, 0x2, "empty", nil_talk);
	uxn_port(&u, 0x3, "empty", nil_talk);
	uxn_port(&u, 0x4, "empty", nil_talk);
	uxn_port(&u, 0x5, "empty", nil_talk);
	uxn_port(&u, 0x6, "empty", nil_talk);
	uxn_port(&u, 0x7, "empty", nil_talk);
	uxn_port(&u, 0x8, "empty", nil_talk);
	uxn_port(&u, 0x9, "empty", nil_talk);
	uxn_port(&u, 0xa, "file", file_talk);
	uxn_port(&u, 0xb, "datetime", datetime_talk);
	uxn_port(&u, 0xc, "empty", nil_talk);
	uxn_port(&u, 0xd, "empty", nil_talk);
	uxn_port(&u, 0xe, "empty", nil_talk);
	uxn_port(&u, 0xf, "empty", nil_talk);

	run(&u);

	return 0;
}
