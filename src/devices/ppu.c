#include "ppu.h"

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

void
clear(Ppu *p)
{
	int i, sz = p->height * p->width;
	for(i = 0; i < sz; ++i) {
		p->pixels[i] = 0;
	}
}

void
putpixel(Ppu *p, Uint8 layer, Uint16 x, Uint16 y, Uint8 color)
{
	Uint8 *pixel = &p->pixels[y * p->width + x], shift = layer * 2;
	if(x < p->width && y < p->height)
		*pixel = (*pixel & ~(0x3 << shift)) | (color << shift);
}

void
puticn(Ppu *p, Uint8 layer, Uint16 x, Uint16 y, Uint8 *sprite, Uint8 color, Uint8 flipx, Uint8 flipy)
{
	Uint16 v, h;
	for(v = 0; v < 8; v++)
		for(h = 0; h < 8; h++) {
			Uint8 ch1 = (sprite[v] >> (7 - h)) & 0x1;
			if(ch1 == 1 || (color != 0x05 && color != 0x0a && color != 0x0f))
				putpixel(p,
					layer,
					x + (flipx ? 7 - h : h),
					y + (flipy ? 7 - v : v),
					ch1 ? color % 4 : color / 4);
		}
}

void
putchr(Ppu *p, Uint8 layer, Uint16 x, Uint16 y, Uint8 *sprite, Uint8 color, Uint8 flipx, Uint8 flipy)
{
	Uint8 k1 = 1, k2 = 2;
	Uint16 v, h;
	if(!(color & 1)) {
		++color;
		k1 = 2, k2 = 1;
	}
	for(v = 0; v < 8; v++)
		for(h = 0; h < 8; h++) {
			Uint8 ch1 = ((sprite[v] >> (7 - h)) & 0x1) * color;
			Uint8 ch2 = ((sprite[v + 8] >> (7 - h)) & 0x1) * color;
			putpixel(p,
				layer,
				x + (flipx ? 7 - h : h),
				y + (flipy ? 7 - v : v),
				((ch1 * k1 + ch2 * k2) + color / 4) & 0x3);
		}
}

/* output */

int
initppu(Ppu *p, Uint8 hor, Uint8 ver)
{
	p->hor = hor;
	p->ver = ver;
	p->width = 8 * p->hor;
	p->height = 8 * p->ver;
	return 1;
}
