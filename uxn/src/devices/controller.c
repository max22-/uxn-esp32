#include "../uxn.h"
#include "controller.h"

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
controller_down(Device *d, Uint8 mask)
{
	if(mask) {
		d->dat[2] |= mask;
		uxn_eval(d->u, GETVECTOR(d));
	}
}

void
controller_up(Device *d, Uint8 mask)
{
	if(mask) {
		d->dat[2] &= (~mask);
		uxn_eval(d->u, GETVECTOR(d));
	}
}

void
controller_key(Device *d, Uint8 key)
{
	if(key) {
		d->dat[3] = key;
		uxn_eval(d->u, GETVECTOR(d));
		d->dat[3] = 0x00;
	}
}

void
controller_special(Device *d, Uint8 key)
{
	if(key) {
		d->dat[4] = key;
		uxn_eval(d->u, GETVECTOR(d));
		d->dat[4] = 0x00;
	}
}
