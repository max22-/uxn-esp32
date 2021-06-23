#ifdef __plan9__
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#else
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
typedef uint8_t u8int;
typedef uint32_t u32int;
#define nil NULL
typedef struct {
}Memimage;
typedef struct {
	u32int *base;
	u8int *bdata;
}Memdata;
static char *argv0;
#define fprint(x, arg...) fprintf(stderr, arg)
#define exits(s) exit(s == NULL ? 0 : 1)
#define sysfatal(s) do{ fprintf(stderr, "error\n"); exit(1); }while(0)
#define	ARGBEGIN \
	for(((argv0=*argv)),argv++,argc--; \
		argv[0] && argv[0][0]=='-' && argv[0][1]; \
		argc--, argv++){ \
			char *_args, _argc, *_argt; \
			_args = &argv[0][1]; \
			if(_args[0]=='-' && _args[1]==0){ \
				argc--; argv++; break; \
			} \
			_argc = 0; \
			while(*_args && (_argc = *_args++)) \
			switch(_argc)
#define	ARGEND };
#define	EARGF(x)\
	(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): ((x), abort(), (char*)0)))
#endif

static int hor = 44, ver = 26, bpp = 1;

#define xy2n(x, y) ((y & 7) + (x/8 + y/8 * hor)*bpp*8)

static u8int *
readall(int f, int *isz)
{
	int bufsz, sz, n;
	u8int *s;

	bufsz = 1023;
	s = nil;
	for(sz = 0;; sz += n){
		if(bufsz-sz < 1024){
			bufsz *= 2;
			s = realloc(s, bufsz);
		}
		if((n = read(f, s+sz, bufsz-sz)) < 1)
			break;
	}
	if(n < 0 || sz < 1){
		free(s);
		return nil;
	}
	*isz = sz;

	return s;
}

static int
getcoli(int x, int y, u8int *p)
{
	int ch1, ch2, r;

	r = xy2n(x, y);
	ch1 = (p[r+0] >> (7 - (x & 7))) & 1;
	ch2 = bpp < 2 ? 0 : (p[r+8] >> (7 - (x & 7))) & 1;

	return ch2<<1 | ch1;
}

static int
writebmp(int w, int h, u32int *p)
{
	u8int hd[14+40+4*4] = {
		'B', 'M',
		0, 0, 0, 0, /* file size */
		0, 0,
		0, 0,
		14+40+4*4, 0, 0, 0, /* pixel data offset */
		40+4*4, 0, 0, 0, /* BITMAPINFOHEADER */
		w, w>>8, 0, 0, /* width */
		h, h>>8, 0, 0, /* height */
		1, 0, /* color planes */
		32, 0, /* bpp = rgba */
		3, 0, 0, 0, /* no compression */
		0, 0, 0, 0, /* dummy raw size */
		0, 0, 0, 0, /* dummy hor ppm */
		0, 0, 0, 0, /* dummy ver ppm */
		0, 0, 0, 0, /* dummy num of colors */
		0, 0, 0, 0, /* dummy important colors */
		0, 0, 0, 0xff,
		0, 0, 0xff, 0,
		0, 0xff, 0, 0,
		0xff, 0, 0, 0,
	};
	int sz;

	sz = 14+40+4*4 + 4*w*h;
	hd[2] = sz;
	hd[3] = sz>>8;
	hd[4] = sz>>16;
	hd[5] = sz>>24;

	write(1, hd, sizeof(hd));
	while(h-- >= 0)
		write(1, p+w*h, 4*w);

	return 0;
}

static void
usage(void)
{
	fprint(2, "usage: %s [-1] [-2] [-w WIDTH]\n", argv0);
	exits("usage");
}

int
main(int argc, char **argv)
{
	int sz, esz, h, w, x, y;
	Memimage *m;
	Memdata d;
	u8int *p;
	u32int col[2][4] = {
		{0xffffffff, 0x000000ff, 0x000000ff, 0x000000ff},
		{0xffffff00, 0xffffffff, 0x72dec2ff, 0x666666ff},
	};

	ARGBEGIN{
	case '1':
		bpp = 1;
		break;
	case '2':
		bpp = 2;
		break;
	case 'w':
		hor = atoi(EARGF(usage()));
		break;
	default:
		usage();
	}ARGEND

	if((p = readall(0, &sz)) == nil)
		sysfatal("%r");

	ver = sz / (bpp*8) / hor;
	esz = (hor * ver * (bpp*8));
	w = hor*8;
	h = ver*8;
	if(sz != esz)
		fprint(2, "warning: size differs (%d vs %d), dimensions must be wrong\n", sz, esz);

	memset(&d, 0, sizeof(d));
	if((d.base = malloc(4*w*h)) == nil)
		sysfatal("memory");
	d.bdata = (u8int*)d.base;

	for(y = 0; y < h; y++){
		for(x = 0; x < w; x++)
			d.base[y*w + x] = col[bpp-1][getcoli(x, y, p)];
	}

#ifdef __plan9__
	memimageinit();
	if((m = allocmemimaged(Rect(0, 0, w, h), RGBA32, &d)) == nil)
		sysfatal("%r");
	if(writememimage(1, m) != 0)
		sysfatal("%r");
#else
	(void)m;
	writebmp(w, h, d.base);
#endif

	exits(nil);
	return 0;
}
