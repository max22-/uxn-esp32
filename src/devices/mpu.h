#include <stdio.h>
#include <stdlib.h>

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#ifndef NO_PORTMIDI
#include <portmidi.h>
#include <porttime.h>
#else
typedef struct {
	int message;
} PmEvent;
#endif

typedef unsigned char Uint8;

typedef struct {
	Uint8 queue;
	PmEvent events[32];
#ifndef NO_PORTMIDI
	PmStream *input, *output;
	PmError error;
#endif
} Mpu;

int initmpu(Mpu *m, Uint8 dev_in, Uint8 dev_out);
void getmidi(Mpu *m);
void putmidi(Mpu *m, Uint8 chan, Uint8 note, Uint8 velo);