#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "uxn.h"

#pragma GCC diagnostic push
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma clang diagnostic ignored "-Wtypedef-redefinition"
#include <SDL.h>
#include "devices/system.h"
#include "devices/screen.h"
#include "devices/audio.h"
#include "devices/file.h"
#include "devices/controller.h"
#include "devices/mouse.h"
#include "devices/datetime.h"
#ifdef _WIN32
#include <processthreadsapi.h>
#endif
#pragma GCC diagnostic pop
#pragma clang diagnostic pop

/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define WIDTH 64 * 8
#define HEIGHT 40 * 8
#define PAD 4
#define TIMEOUT_MS 334
#define BENCH 0

static SDL_Window *gWindow;
static SDL_Texture *gTexture;
static SDL_Renderer *gRenderer;
static SDL_AudioDeviceID audio_id;
static SDL_Rect gRect;
static SDL_Thread *stdin_thread;

/* devices */

static Device *devscreen, *devmouse, *devctrl, *devaudio0;
static Uint8 zoom = 1;
static Uint32 stdin_event, audio0_event;
static Uint64 exec_deadline, deadline_interval, ms_interval;

static int
error(char *msg, const char *err)
{
	fprintf(stderr, "%s: %s\n", msg, err);
	fflush(stderr);
	return 0;
}

#pragma mark - Generics

static void
audio_callback(void *u, Uint8 *stream, int len)
{
	int instance, running = 0;
	Sint16 *samples = (Sint16 *)stream;
	SDL_memset(stream, 0, len);
	for(instance = 0; instance < POLYPHONY; instance++)
		running += audio_render(instance, samples, samples + len / 2);
	if(!running)
		SDL_PauseAudioDevice(audio_id, 1);
	(void)u;
}

void
audio_finished_handler(int instance)
{
	SDL_Event event;
	event.type = audio0_event + instance;
	SDL_PushEvent(&event);
}

static int
stdin_handler(void *p)
{
	SDL_Event event;
	event.type = stdin_event;
	while(read(0, &event.cbutton.button, 1) > 0 && SDL_PushEvent(&event) >= 0)
		;
	return 0;
	(void)p;
}

static void
set_window_size(SDL_Window *window, int w, int h)
{
	SDL_Point win, win_old;
	SDL_GetWindowPosition(window, &win.x, &win.y);
	SDL_GetWindowSize(window, &win_old.x, &win_old.y);
	if(w == win_old.x && h == win_old.y) return;
	SDL_SetWindowPosition(window, (win.x + win_old.x / 2) - w / 2, (win.y + win_old.y / 2) - h / 2);
	SDL_SetWindowSize(window, w, h);
}

int
set_size(void)
{
	gRect.x = PAD;
	gRect.y = PAD;
	gRect.w = uxn_screen.width;
	gRect.h = uxn_screen.height;
	if(gTexture != NULL) SDL_DestroyTexture(gTexture);
	SDL_RenderSetLogicalSize(gRenderer, uxn_screen.width + PAD * 2, uxn_screen.height + PAD * 2);
	gTexture = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STATIC, uxn_screen.width, uxn_screen.height);
	if(gTexture == NULL || SDL_SetTextureBlendMode(gTexture, SDL_BLENDMODE_NONE))
		return error("gTexture", SDL_GetError());
	if(SDL_UpdateTexture(gTexture, NULL, uxn_screen.pixels, sizeof(Uint32)) != 0)
		return error("SDL_UpdateTexture", SDL_GetError());
	set_window_size(gWindow, (uxn_screen.width + PAD * 2) * zoom, (uxn_screen.height + PAD * 2) * zoom);
	return 1;
}

static void
redraw(void)
{
	if(gRect.w != uxn_screen.width || gRect.h != uxn_screen.height) set_size();
	screen_redraw(&uxn_screen, uxn_screen.pixels);
	if(SDL_UpdateTexture(gTexture, NULL, uxn_screen.pixels, uxn_screen.width * sizeof(Uint32)) != 0)
		error("SDL_UpdateTexture", SDL_GetError());
	SDL_RenderClear(gRenderer);
	SDL_RenderCopy(gRenderer, gTexture, NULL, &gRect);
	SDL_RenderPresent(gRenderer);
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
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0)
		return error("sdl", SDL_GetError());
	gWindow = SDL_CreateWindow("Uxn", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (WIDTH + PAD * 2) * zoom, (HEIGHT + PAD * 2) * zoom, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
	if(gWindow == NULL)
		return error("sdl_window", SDL_GetError());
	gRenderer = SDL_CreateRenderer(gWindow, -1, 0);
	if(gRenderer == NULL)
		return error("sdl_renderer", SDL_GetError());
	SDL_SetRenderDrawColor(gRenderer, 0x00, 0x00, 0x00, 0xff);
	audio_id = SDL_OpenAudioDevice(NULL, 0, &as, NULL, 0);
	if(!audio_id)
		error("sdl_audio", SDL_GetError());
	if(SDL_NumJoysticks() > 0 && SDL_JoystickOpen(0) == NULL)
		error("sdl_joystick", SDL_GetError());
	stdin_event = SDL_RegisterEvents(1);
	audio0_event = SDL_RegisterEvents(POLYPHONY);
	SDL_DetachThread(stdin_thread = SDL_CreateThread(stdin_handler, "stdin", NULL));
	SDL_StartTextInput();
	SDL_ShowCursor(SDL_DISABLE);
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	ms_interval = SDL_GetPerformanceFrequency() / 1000;
	deadline_interval = ms_interval * TIMEOUT_MS;
	return 1;
}

#pragma mark - Devices

void
system_deo_special(Device *d, Uint8 port)
{
	if(port > 0x7 && port < 0xe)
		screen_palette(&uxn_screen, &d->dat[0x8]);
}

static void
console_deo(Device *d, Uint8 port)
{
	FILE *fd = port == 0x8 ? stdout : port == 0x9 ? stderr
												  : 0;
	if(fd) {
		fputc(d->dat[port], fd);
		fflush(fd);
	}
}

static Uint8
audio_dei(Device *d, Uint8 port)
{
	int instance = d - devaudio0;
	if(!audio_id) return d->dat[port];
	switch(port) {
	case 0x4: return audio_get_vu(instance);
	case 0x2: DEVPOKE16(0x2, audio_get_position(instance)); /* fall through */
	default: return d->dat[port];
	}
}

static void
audio_deo(Device *d, Uint8 port)
{
	int instance = d - devaudio0;
	if(!audio_id) return;
	if(port == 0xf) {
		SDL_LockAudioDevice(audio_id);
		audio_start(instance, d);
		SDL_UnlockAudioDevice(audio_id);
		SDL_PauseAudioDevice(audio_id, 0);
	}
}

static Uint8
nil_dei(Device *d, Uint8 port)
{
	return d->dat[port];
}

static void
nil_deo(Device *d, Uint8 port)
{
	(void)d;
	(void)port;
}

/* Boot */

static int
load(Uxn *u, char *rom)
{
	SDL_RWops *f;
	int r;
	if(!(f = SDL_RWFromFile(rom, "rb"))) return 0;
	r = f->read(f, u->ram + PAGE_PROGRAM, 1, 0x10000 - PAGE_PROGRAM);
	f->close(f);
	if(r < 1) return 0;
	fprintf(stderr, "Loaded %s\n", rom);
	fflush(stderr);
	SDL_SetWindowTitle(gWindow, rom);
	return 1;
}

static int
start(Uxn *u, char *rom)
{
	free(u->ram);
	if(!uxn_boot(u, calloc(0x10000, 1)))
		return error("Boot", "Failed to start uxn.");
	if(!load(u, rom))
		return error("Boot", "Failed to load rom.");
	/* system   */ uxn_port(u, 0x0, system_dei, system_deo);
	/* console  */ uxn_port(u, 0x1, nil_dei, console_deo);
	/* screen   */ devscreen = uxn_port(u, 0x2, screen_dei, screen_deo);
	/* audio0   */ devaudio0 = uxn_port(u, 0x3, audio_dei, audio_deo);
	/* audio1   */ uxn_port(u, 0x4, audio_dei, audio_deo);
	/* audio2   */ uxn_port(u, 0x5, audio_dei, audio_deo);
	/* audio3   */ uxn_port(u, 0x6, audio_dei, audio_deo);
	/* unused   */ uxn_port(u, 0x7, nil_dei, nil_deo);
	/* control  */ devctrl = uxn_port(u, 0x8, nil_dei, nil_deo);
	/* mouse    */ devmouse = uxn_port(u, 0x9, nil_dei, nil_deo);
	/* file0    */ uxn_port(u, 0xa, file_dei, file_deo);
	/* file1    */ uxn_port(u, 0xb, file_dei, file_deo);
	/* datetime */ uxn_port(u, 0xc, datetime_dei, nil_deo);
	/* unused   */ uxn_port(u, 0xd, nil_dei, nil_deo);
	/* unused   */ uxn_port(u, 0xe, nil_dei, nil_deo);
	/* unused   */ uxn_port(u, 0xf, nil_dei, nil_deo);
	exec_deadline = SDL_GetPerformanceCounter() + deadline_interval;
	if(!uxn_eval(u, PAGE_PROGRAM))
		return error("Boot", "Failed to start rom.");
	return 1;
}

static void
set_zoom(Uint8 scale)
{
	zoom = clamp(scale, 1, 3);
	set_window_size(gWindow, (uxn_screen.width + PAD * 2) * zoom, (uxn_screen.height + PAD * 2) * zoom);
}

static void
capture_screen(void)
{
	const Uint32 format = SDL_PIXELFORMAT_RGB24;
	time_t t = time(NULL);
	char fname[64];
	int w, h;
	SDL_Surface *surface;
	SDL_GetRendererOutputSize(gRenderer, &w, &h);
	surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, format);
	SDL_RenderReadPixels(gRenderer, NULL, format, surface->pixels, surface->pitch);
	strftime(fname, sizeof(fname), "screenshot-%Y%m%d-%H%M%S.bmp", localtime(&t));
	SDL_SaveBMP(surface, fname);
	SDL_FreeSurface(surface);
	fprintf(stderr, "Saved %s\n", fname);
	fflush(stderr);
}

static void
restart(Uxn *u)
{
	screen_resize(&uxn_screen, WIDTH, HEIGHT);
	start(u, "launcher.rom");
}

static Uint8
get_button(SDL_Event *event)
{
	switch(event->key.keysym.sym) {
	case SDLK_LCTRL: return 0x01;
	case SDLK_LALT: return 0x02;
	case SDLK_LSHIFT: return 0x04;
	case SDLK_HOME: return 0x08;
	case SDLK_UP: return 0x10;
	case SDLK_DOWN: return 0x20;
	case SDLK_LEFT: return 0x40;
	case SDLK_RIGHT: return 0x80;
	}
	return 0x00;
}

static Uint8
get_button_joystick(SDL_Event *event)
{
	return 0x01 << (event->jbutton.button & 0x3);
}

static Uint8
get_vector_joystick(SDL_Event *event)
{
	if(event->jaxis.value < -3200)
		return 1;
	if(event->jaxis.value > 3200)
		return 2;
	return 0;
}

static Uint8
get_key(SDL_Event *event)
{
	SDL_Keymod mods = SDL_GetModState();
	if(event->key.keysym.sym < 0x20 || event->key.keysym.sym == SDLK_DELETE)
		return event->key.keysym.sym;
	if((mods & KMOD_CTRL) && event->key.keysym.sym >= SDLK_a && event->key.keysym.sym <= SDLK_z)
		return event->key.keysym.sym - (mods & KMOD_SHIFT) * 0x20;
	return 0x00;
}

static void
do_shortcut(Uxn *u, SDL_Event *event)
{
	if(event->key.keysym.sym == SDLK_F1)
		set_zoom(zoom > 2 ? 1 : zoom + 1);
	else if(event->key.keysym.sym == SDLK_F2)
		system_inspect(u);
	else if(event->key.keysym.sym == SDLK_F3)
		capture_screen();
	else if(event->key.keysym.sym == SDLK_F4)
		restart(u);
	else if(event->key.keysym.sym == SDLK_F5) {
		screen_mono(&uxn_screen, uxn_screen.pixels);
		redraw();
	}
}

static int
console_input(Uxn *u, char c)
{
	Device *d = &u->dev[1];
	d->dat[0x2] = c;
	return uxn_eval(u, GETVECTOR(d));
}

static int
handle_events(Uxn *u)
{
	SDL_Event event;
	while(SDL_PollEvent(&event)) {
		/* Window */
		if(event.type == SDL_QUIT)
			return error("Run", "Quit.");
		else if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_EXPOSED)
			redraw();
		else if(event.type == SDL_DROPFILE) {
			screen_resize(&uxn_screen, WIDTH, HEIGHT);
			start(u, event.drop.file);
			SDL_free(event.drop.file);
		}
		/* Audio */
		else if(event.type >= audio0_event && event.type < audio0_event + POLYPHONY) {
			Device *d = devaudio0 + (event.type - audio0_event);
			uxn_eval(u, GETVECTOR(d));
		}
		/* Mouse */
		else if(event.type == SDL_MOUSEMOTION)
			mouse_pos(devmouse,
				clamp(event.motion.x - PAD, 0, uxn_screen.width - 1),
				clamp(event.motion.y - PAD, 0, uxn_screen.height - 1));
		else if(event.type == SDL_MOUSEBUTTONUP)
			mouse_up(devmouse, SDL_BUTTON(event.button.button));
		else if(event.type == SDL_MOUSEBUTTONDOWN)
			mouse_down(devmouse, SDL_BUTTON(event.button.button));
		else if(event.type == SDL_MOUSEWHEEL)
			mouse_scroll(devmouse, event.wheel.x, event.wheel.y);
		/* Controller */
		else if(event.type == SDL_TEXTINPUT)
			controller_key(devctrl, event.text.text[0]);
		else if(event.type == SDL_KEYDOWN) {
			int ksym;
			if(get_key(&event))
				controller_key(devctrl, get_key(&event));
			else if(get_button(&event))
				controller_down(devctrl, get_button(&event));
			else
				do_shortcut(u, &event);
			ksym = event.key.keysym.sym;
			if(SDL_PeepEvents(&event, 1, SDL_PEEKEVENT, SDL_KEYUP, SDL_KEYUP) == 1 && ksym == event.key.keysym.sym) {
				return 1;
			}
		} else if(event.type == SDL_KEYUP)
			controller_up(devctrl, get_button(&event));
		else if(event.type == SDL_JOYAXISMOTION) {
			Uint8 vec = get_vector_joystick(&event);
			if(!vec)
				controller_up(devctrl, (0x03 << (!event.jaxis.axis * 2)) << 4);
			else
				controller_down(devctrl, (0x01 << ((vec + !event.jaxis.axis * 2) - 1)) << 4);
		} else if(event.type == SDL_JOYBUTTONDOWN)
			controller_down(devctrl, get_button_joystick(&event));
		else if(event.type == SDL_JOYBUTTONUP)
			controller_up(devctrl, get_button_joystick(&event));
		/* Console */
		else if(event.type == stdin_event)
			console_input(u, event.cbutton.button);
	}
	return 1;
}

static int
run(Uxn *u)
{
	Device *devsys = &u->dev[0];
	Uint64 now = SDL_GetPerformanceCounter(), frame_end, frame_interval = SDL_GetPerformanceFrequency() / 60;
	for(;;) {
		/* .System/halt */
		if(devsys->dat[0xf])
			return error("Run", "Ended.");
		frame_end = now + frame_interval;
		exec_deadline = now + deadline_interval;
		if(!handle_events(u))
			return 0;
		uxn_eval(u, GETVECTOR(devscreen));
		if(uxn_screen.fg.changed || uxn_screen.bg.changed)
			redraw();
		now = SDL_GetPerformanceCounter();
		if(!BENCH && ((Sint64)(frame_end - now)) > 0) {
			SDL_Delay((frame_end - now) / ms_interval);
			now = frame_end;
		}
	}
	return error("SDL_WaitEvent", SDL_GetError());
}

int
uxn_interrupt(void)
{
	return !SDL_QuitRequested();
}

int
main(int argc, char **argv)
{
	SDL_DisplayMode DM;
	Uxn u = {0};
	int i, loaded = 0;

	if(!init())
		return error("Init", "Failed to initialize emulator.");
	screen_resize(&uxn_screen, WIDTH, HEIGHT);

	/* set default zoom */
	if(SDL_GetCurrentDisplayMode(0, &DM) == 0)
		set_zoom(DM.w / 1280);
	for(i = 1; i < argc; i++) {
		/* get default zoom from flags */
		if(strcmp(argv[i], "-s") == 0) {
			if(i < argc - 1)
				set_zoom(atoi(argv[++i]));
			else
				return error("Opt", "-s No scale provided.");
		} else if(!loaded++) {
			if(!start(&u, argv[i]))
				return error("Boot", "Failed to boot.");
		} else {
			char *p = argv[i];
			while(*p) console_input(&u, *p++);
			console_input(&u, '\n');
		}
	}
	if(!loaded && !start(&u, "launcher.rom"))
		return error("usage", "uxnemu [-s scale] file.rom");
	run(&u);
#ifdef _WIN32
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
	TerminateThread((HANDLE)SDL_GetThreadID(stdin_thread), 0);
#elif !defined(__APPLE__)
	close(0); /* make stdin thread exit */
#endif
	SDL_Quit();
	return 0;
}
