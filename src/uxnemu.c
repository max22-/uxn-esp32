#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "uxn.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <SDL.h>
#include "devices/ppu.h"
#include "devices/apu.h"
#pragma GCC diagnostic pop

/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define PAD 4
#define POLYPHONY 4
#define BENCH 0

static SDL_AudioDeviceID audio_id;
static SDL_Window *gWindow;
static SDL_Surface *winSurface, *idxSurface, *rgbaSurface;
static SDL_Rect gRect;
static Ppu ppu;
static Apu apu[POLYPHONY];
static Device *devsystem, *devscreen, *devmouse, *devctrl, *devaudio0, *devconsole;
static Uint32 stdin_event;

static Uint8 zoom = 1, reqdraw = 0;

static Uint8 font[][8] = {
	{0x00, 0x7c, 0x82, 0x82, 0x82, 0x82, 0x82, 0x7c},
	{0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
	{0x00, 0x7c, 0x82, 0x02, 0x7c, 0x80, 0x80, 0xfe},
	{0x00, 0x7c, 0x82, 0x02, 0x1c, 0x02, 0x82, 0x7c},
	{0x00, 0x0c, 0x14, 0x24, 0x44, 0x84, 0xfe, 0x04},
	{0x00, 0xfe, 0x80, 0x80, 0x7c, 0x02, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x80, 0xfc, 0x82, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x02, 0x1e, 0x02, 0x02, 0x02},
	{0x00, 0x7c, 0x82, 0x82, 0x7c, 0x82, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x82, 0x7e, 0x02, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x02, 0x7e, 0x82, 0x82, 0x7e},
	{0x00, 0xfc, 0x82, 0x82, 0xfc, 0x82, 0x82, 0xfc},
	{0x00, 0x7c, 0x82, 0x80, 0x80, 0x80, 0x82, 0x7c},
	{0x00, 0xfc, 0x82, 0x82, 0x82, 0x82, 0x82, 0xfc},
	{0x00, 0x7c, 0x82, 0x80, 0xf0, 0x80, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x80, 0xf0, 0x80, 0x80, 0x80}};

static int
clamp(int val, int min, int max)
{
	return (val >= min) ? (val <= max) ? val : max : min;
}

static int
error(char *msg, const char *err)
{
	fprintf(stderr, "%s: %s\n", msg, err);
	return 0;
}

static void
audio_callback(void *u, Uint8 *stream, int len)
{
	int i, running = 0;
	Sint16 *samples = (Sint16 *)stream;
	SDL_memset(stream, 0, len);
	for(i = 0; i < POLYPHONY; ++i)
		running += apu_render(&apu[i], samples, samples + len / 2);
	if(!running)
		SDL_PauseAudioDevice(audio_id, 1);
	(void)u;
}

static void
inspect(Ppu *p, Uint8 *stack, Uint8 wptr, Uint8 rptr, Uint8 *memory)
{
	Uint8 i, x, y, b;
	for(i = 0; i < 0x20; ++i) { /* stack */
		x = ((i % 8) * 3 + 1) * 8, y = (i / 8 + 1) * 8, b = stack[i];
		ppu_1bpp(p, 1, x, y, font[(b >> 4) & 0xf], 1 + (wptr == i) * 0x7, 0, 0);
		ppu_1bpp(p, 1, x + 8, y, font[b & 0xf], 1 + (wptr == i) * 0x7, 0, 0);
	}
	/* return pointer */
	ppu_1bpp(p, 1, 0x8, y + 0x10, font[(rptr >> 4) & 0xf], 0x2, 0, 0);
	ppu_1bpp(p, 1, 0x10, y + 0x10, font[rptr & 0xf], 0x2, 0, 0);
	for(i = 0; i < 0x20; ++i) { /* memory */
		x = ((i % 8) * 3 + 1) * 8, y = 0x38 + (i / 8 + 1) * 8, b = memory[i];
		ppu_1bpp(p, 1, x, y, font[(b >> 4) & 0xf], 3, 0, 0);
		ppu_1bpp(p, 1, x + 8, y, font[b & 0xf], 3, 0, 0);
	}
	for(x = 0; x < 0x10; ++x) { /* guides */
		ppu_pixel(p, 1, x, p->height / 2, 2);
		ppu_pixel(p, 1, p->width - x, p->height / 2, 2);
		ppu_pixel(p, 1, p->width / 2, p->height - x, 2);
		ppu_pixel(p, 1, p->width / 2, x, 2);
		ppu_pixel(p, 1, p->width / 2 - 0x10 / 2 + x, p->height / 2, 2);
		ppu_pixel(p, 1, p->width / 2, p->height / 2 - 0x10 / 2 + x, 2);
	}
}

static void
redraw(Uxn *u)
{
	if(devsystem->dat[0xe])
		inspect(&ppu, u->wst.dat, u->wst.ptr, u->rst.ptr, u->ram.dat);
	if(rgbaSurface == NULL)
		SDL_BlitScaled(idxSurface, NULL, winSurface, &gRect);
	else if(zoom == 1)
		SDL_BlitSurface(idxSurface, NULL, winSurface, &gRect);
	else {
		SDL_BlitSurface(idxSurface, NULL, rgbaSurface, NULL);
		SDL_BlitScaled(rgbaSurface, NULL, winSurface, &gRect);
	}
	SDL_UpdateWindowSurface(gWindow);
	reqdraw = 0;
}

static void
toggledebug(Uxn *u)
{
	devsystem->dat[0xe] = !devsystem->dat[0xe];
	redraw(u);
}

static void
togglezoom(Uxn *u)
{
	zoom = zoom == 3 ? 1 : zoom + 1;
	SDL_SetWindowSize(gWindow, (ppu.width + PAD * 2) * zoom, (ppu.height + PAD * 2) * zoom);
	winSurface = SDL_GetWindowSurface(gWindow);
	gRect.x = zoom * PAD;
	gRect.y = zoom * PAD;
	gRect.w = zoom * ppu.width;
	gRect.h = zoom * ppu.height;
	redraw(u);
}

static void
screencapture(void)
{
	time_t t = time(NULL);
	char fname[64];
	strftime(fname, sizeof(fname), "screenshot-%Y%m%d-%H%M%S.bmp", localtime(&t));
	SDL_SaveBMP(winSurface, fname);
	fprintf(stderr, "Saved %s\n", fname);
}

static void
quit(void)
{
	SDL_UnlockAudioDevice(audio_id);
	SDL_FreeSurface(winSurface);
	SDL_FreeSurface(idxSurface);
	if(rgbaSurface) SDL_FreeSurface(rgbaSurface);
	SDL_DestroyWindow(gWindow);
	SDL_Quit();
	exit(0);
}

static int
init(void)
{
	SDL_AudioSpec as;
	SDL_zero(as);
	as.freq = SAMPLE_FREQUENCY;
	as.format = AUDIO_S16;
	as.channels = 2;
	as.callback = audio_callback;
	as.samples = 512;
	as.userdata = NULL;
	if(!ppu_init(&ppu, 64, 40))
		return error("ppu", "Init failure");
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		error("sdl", SDL_GetError());
		if(SDL_Init(SDL_INIT_VIDEO) < 0)
			return error("sdl", SDL_GetError());
	} else {
		audio_id = SDL_OpenAudioDevice(NULL, 0, &as, NULL, 0);
		if(!audio_id)
			error("sdl_audio", SDL_GetError());
	}
	gWindow = SDL_CreateWindow("Uxn", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (ppu.width + PAD * 2) * zoom, (ppu.height + PAD * 2) * zoom, SDL_WINDOW_SHOWN);
	if(gWindow == NULL)
		return error("sdl_window", SDL_GetError());
	winSurface = SDL_GetWindowSurface(gWindow);
	if(winSurface == NULL)
		return error("sdl_surface win", SDL_GetError());
	idxSurface = SDL_CreateRGBSurfaceWithFormat(0, ppu.width, ppu.height, 8, SDL_PIXELFORMAT_INDEX8);
	if(idxSurface == NULL || SDL_SetSurfaceBlendMode(idxSurface, SDL_BLENDMODE_NONE))
		return error("sdl_surface idx", SDL_GetError());
	if(SDL_MUSTLOCK(idxSurface) == SDL_TRUE)
		return error("sdl_surface idx", "demands locking");
	gRect.x = zoom * PAD;
	gRect.y = zoom * PAD;
	gRect.w = zoom * ppu.width;
	gRect.h = 2 * ppu.height; /* force non-1:1 scaling for BlitScaled test */
	if(SDL_BlitScaled(idxSurface, NULL, winSurface, &gRect) < 0) {
		rgbaSurface = SDL_CreateRGBSurfaceWithFormat(0, ppu.width, ppu.height, 32, SDL_PIXELFORMAT_RGB24);
		if(rgbaSurface == NULL || SDL_SetSurfaceBlendMode(rgbaSurface, SDL_BLENDMODE_NONE))
			return error("sdl_surface rgba", SDL_GetError());
	}
	gRect.h = zoom * ppu.height;
	ppu.pixels = idxSurface->pixels;
	SDL_StartTextInput();
	SDL_ShowCursor(SDL_DISABLE);
	return 1;
}

static void
domouse(SDL_Event *event)
{
	Uint8 flag = 0x00;
	Uint16 x = clamp(event->motion.x / zoom - PAD, 0, ppu.width - 1);
	Uint16 y = clamp(event->motion.y / zoom - PAD, 0, ppu.height - 1);
	mempoke16(devmouse->dat, 0x2, x);
	mempoke16(devmouse->dat, 0x4, y);
	switch(event->button.button) {
	case SDL_BUTTON_LEFT: flag = 0x01; break;
	case SDL_BUTTON_RIGHT: flag = 0x10; break;
	}
	switch(event->type) {
	case SDL_MOUSEBUTTONDOWN:
		devmouse->dat[6] |= flag;
		break;
	case SDL_MOUSEBUTTONUP:
		devmouse->dat[6] &= (~flag);
		break;
	}
}

static void
doctrl(Uxn *u, SDL_Event *event, int z)
{
	Uint8 flag = 0x00;
	SDL_Keymod mods = SDL_GetModState();
	devctrl->dat[2] &= 0xf8;
	if(mods & KMOD_CTRL) devctrl->dat[2] |= 0x01;
	if(mods & KMOD_ALT) devctrl->dat[2] |= 0x02;
	if(mods & KMOD_SHIFT) devctrl->dat[2] |= 0x04;
	/* clang-format off */
	switch(event->key.keysym.sym) {
	case SDLK_ESCAPE: flag = 0x08; break;
	case SDLK_UP: flag = 0x10; break;
	case SDLK_DOWN: flag = 0x20; break;
	case SDLK_LEFT: flag = 0x40; break;
	case SDLK_RIGHT: flag = 0x80; break;
	case SDLK_F1: if(z) togglezoom(u); break;
	case SDLK_F2: if(z) toggledebug(u); break;
	case SDLK_F3: if(z) screencapture(); break;
	}

	/* clang-format on */
	if(z) {
		devctrl->dat[2] |= flag;
		if(event->key.keysym.sym < 0x20 || event->key.keysym.sym == SDLK_DELETE)
			devctrl->dat[3] = event->key.keysym.sym;
		else if((mods & KMOD_CTRL) && event->key.keysym.sym >= SDLK_a && event->key.keysym.sym <= SDLK_z)
			devctrl->dat[3] = event->key.keysym.sym - (mods & KMOD_SHIFT) * 0x20;
	} else
		devctrl->dat[2] &= ~flag;
}

#pragma mark - Devices

static void
system_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(!w) {
		d->dat[0x2] = d->u->wst.ptr;
		d->dat[0x3] = d->u->rst.ptr;
	} else if(b0 > 0x7 && b0 < 0xe) {
		SDL_Color pal[16];
		int i;
		for(i = 0; i < 4; ++i) {
			pal[i].r = ((d->dat[0x8 + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
			pal[i].g = ((d->dat[0xa + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
			pal[i].b = ((d->dat[0xc + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
		}
		for(i = 4; i < 16; ++i) {
			pal[i].r = pal[i / 4].r;
			pal[i].g = pal[i / 4].g;
			pal[i].b = pal[i / 4].b;
		}
		SDL_SetPaletteColors(idxSurface->format->palette, pal, 0, 16);
		reqdraw = 1;
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
screen_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(w && b0 == 0xe) {
		Uint16 x = mempeek16(d->dat, 0x8);
		Uint16 y = mempeek16(d->dat, 0xa);
		Uint8 layer = d->dat[0xe] >> 4 & 0x1;
		ppu_pixel(&ppu, layer, x, y, d->dat[0xe] & 0x3);
		reqdraw = 1;
	} else if(w && b0 == 0xf) {
		Uint16 x = mempeek16(d->dat, 0x8);
		Uint16 y = mempeek16(d->dat, 0xa);
		Uint8 layer = d->dat[0xf] >> 0x6 & 0x1;
		Uint8 *addr = &d->mem[mempeek16(d->dat, 0xc)];
		if(d->dat[0xf] >> 0x7 & 0x1)
			ppu_2bpp(&ppu, layer, x, y, addr, d->dat[0xf] & 0xf, d->dat[0xf] >> 0x4 & 0x1, d->dat[0xf] >> 0x5 & 0x1);
		else
			ppu_1bpp(&ppu, layer, x, y, addr, d->dat[0xf] & 0xf, d->dat[0xf] >> 0x4 & 0x1, d->dat[0xf] >> 0x5 & 0x1);
		reqdraw = 1;
	}
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
audio_talk(Device *d, Uint8 b0, Uint8 w)
{
	Apu *c = &apu[d - devaudio0];
	if(!audio_id) return;
	if(!w) {
		if(b0 == 0x2)
			mempoke16(d->dat, 0x2, c->i);
		else if(b0 == 0x4)
			d->dat[0x4] = apu_get_vu(c);
	} else if(b0 == 0xf) {
		SDL_LockAudioDevice(audio_id);
		c->len = mempeek16(d->dat, 0xa);
		c->addr = &d->mem[mempeek16(d->dat, 0xc)];
		c->volume[0] = d->dat[0xe] >> 4;
		c->volume[1] = d->dat[0xe] & 0xf;
		c->repeat = !(d->dat[0xf] & 0x80);
		apu_start(c, mempeek16(d->dat, 0x8), d->dat[0xf] & 0x7f);
		SDL_UnlockAudioDevice(audio_id);
		SDL_PauseAudioDevice(audio_id, 0);
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

static int
stdin_handler(void *p)
{
	SDL_Event event;
	event.type = stdin_event;
	while(read(0, &event.cbutton.button, 1) > 0)
		SDL_PushEvent(&event);
	return 0;
	(void)p;
}

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
	uxn_eval(u, 0x0100);
	redraw(u);
	while(1) {
		SDL_Event event;
		double elapsed, start = 0;
		if(!BENCH)
			start = SDL_GetPerformanceCounter();
		while(SDL_PollEvent(&event) != 0) {
			switch(event.type) {
			case SDL_QUIT:
				return;
			case SDL_TEXTINPUT:
				devctrl->dat[3] = event.text.text[0]; /* fall-thru */
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				doctrl(u, &event, event.type == SDL_KEYDOWN);
				uxn_eval(u, mempeek16(devctrl->dat, 0));
				devctrl->dat[3] = 0;
				break;
			case SDL_MOUSEWHEEL:
				devmouse->dat[7] = event.wheel.y;
				uxn_eval(u, mempeek16(devmouse->dat, 0));
				devmouse->dat[7] = 0;
				break;
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEMOTION:
				domouse(&event);
				uxn_eval(u, mempeek16(devmouse->dat, 0));
				break;
			case SDL_WINDOWEVENT:
				if(event.window.event == SDL_WINDOWEVENT_EXPOSED)
					redraw(u);
				else if(event.window.event == SDL_WINDOWEVENT_RESIZED)
					winSurface = SDL_GetWindowSurface(gWindow);
				break;
			default:
				if(event.type == stdin_event) {
					devconsole->dat[0x2] = event.cbutton.button;
					uxn_eval(u, mempeek16(devconsole->dat, 0));
				}
			}
		}
		uxn_eval(u, mempeek16(devscreen->dat, 0));
		if(reqdraw || devsystem->dat[0xe])
			redraw(u);
		if(!BENCH) {
			elapsed = (SDL_GetPerformanceCounter() - start) / (double)SDL_GetPerformanceFrequency() * 1000.0f;
			SDL_Delay(clamp(16.666f - elapsed, 0, 1000));
		}
	}
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

	stdin_event = SDL_RegisterEvents(1);
	SDL_CreateThread(stdin_handler, "stdin", NULL);

	if(argc < 2)
		return error("usage", "uxnemu file.rom");
	if(!uxn_boot(&u))
		return error("Boot", "Failed to start uxn.");
	if(!load(&u, argv[1]))
		return error("Load", "Failed to open rom.");
	if(!init())
		return error("Init", "Failed to initialize emulator.");

	devsystem = uxn_port(&u, 0x0, "system", system_talk);
	devconsole = uxn_port(&u, 0x1, "console", console_talk);
	devscreen = uxn_port(&u, 0x2, "screen", screen_talk);
	devaudio0 = uxn_port(&u, 0x3, "audio0", audio_talk);
	uxn_port(&u, 0x4, "audio1", audio_talk);
	uxn_port(&u, 0x5, "audio2", audio_talk);
	uxn_port(&u, 0x6, "audio3", audio_talk);
	uxn_port(&u, 0x7, "---", nil_talk);
	devctrl = uxn_port(&u, 0x8, "controller", nil_talk);
	devmouse = uxn_port(&u, 0x9, "mouse", nil_talk);
	uxn_port(&u, 0xa, "file", file_talk);
	uxn_port(&u, 0xb, "datetime", datetime_talk);
	uxn_port(&u, 0xc, "---", nil_talk);
	uxn_port(&u, 0xd, "---", nil_talk);
	uxn_port(&u, 0xe, "---", nil_talk);
	uxn_port(&u, 0xf, "---", nil_talk);

	/* Write screen size to dev/screen */
	mempoke16(devscreen->dat, 2, ppu.width);
	mempoke16(devscreen->dat, 4, ppu.height);

	run(&u);
	quit();
	return 0;
}
