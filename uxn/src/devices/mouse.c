#include "../uxn.h"
#include "mouse.h"

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
mouse_down(Device *d, Uint8 mask)
{
	d->dat[6] |= mask;
	uxn_eval(d->u, GETVECTOR(d));
}

void
mouse_up(Device *d, Uint8 mask)
{
	d->dat[6] &= (~mask);
	uxn_eval(d->u, GETVECTOR(d));
}

void
mouse_pos(Device *d, Uint16 x, Uint16 y)
{
	DEVPOKE16(0x2, x);
	DEVPOKE16(0x4, y);
	uxn_eval(d->u, GETVECTOR(d));
}

void
mouse_scroll(Device *d, Uint16 x, Uint16 y)
{
	DEVPOKE16(0xa, x);
	DEVPOKE16(0xc, -y);
	uxn_eval(d->u, GETVECTOR(d));
	DEVPOKE16(0xa, 0);
	DEVPOKE16(0xc, 0);
}
