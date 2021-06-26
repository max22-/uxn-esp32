#ifdef ARDUINO
#warning mpu not implemented yet
#else

#include "mpu.h"

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

int
initmpu(Mpu *m, Uint8 dev_in, Uint8 dev_out)
{
#ifndef NO_PORTMIDI
	int i;
	Pm_Initialize();
	for(i = 0; i < Pm_CountDevices(); ++i)
		printf("Device #%d -> %s%s\n", i, Pm_GetDeviceInfo(i)->name, i == dev_in ? "[x]" : "[ ]");
	Pm_OpenInput(&m->input, dev_in, NULL, 128, 0, NULL);
	Pm_OpenOutput(&m->output, dev_out, NULL, 128, 0, NULL, 1);
	m->queue = 0;
	m->error = pmNoError;
#endif
	(void)m;
	(void)dev_in;
	return 1;
}

void
getmidi(Mpu *m)
{
#ifndef NO_PORTMIDI
	const int result = Pm_Read(m->input, m->events, 32);
	if(result < 0) {
		m->error = (PmError)result;
		m->queue = 0;
		return;
	}
	m->queue = result;
#endif
	(void)m;
}

void
putmidi(Mpu *m, Uint8 chan, Uint8 note, Uint8 velo)
{
#ifndef NO_PORTMIDI
	Pm_WriteShort(m->output, Pt_Time(), Pm_Message(0x90 + chan, note, velo));
#endif
}

#endif