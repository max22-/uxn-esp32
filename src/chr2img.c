/* note: this is for Plan 9 only */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

static int hor = 44, ver = 26, bpp = 1;
static int SZ;

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
		if((n = readn(f, s+sz, bufsz-sz)) < 1)
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
	ch1 = (p[r+0] >> (7 - x & 7)) & 1;
	ch2 = bpp < 2 ? 0 : (p[r+8] >> (7 - x & 7)) & 1;

	return ch2<<1 | ch1;
}

static void
usage(void)
{
	fprint(2, "usage: %s [-1] [-2] [-w WIDTH]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int sz, esz, h, w, x, y;
	Memimage *m;
	Memdata d;
	u8int *p;
	ulong col[2][4] = {
		{DWhite, DBlack, 0, 0},
		{DTransparent, DWhite, 0x72dec2ff, 0x666666ff},
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
	}ARGEND

	memimageinit();

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
	d.bdata = (uchar*)d.base;

	for(y = 0; y < h; y++){
		for(x = 0; x < w; x++)
			d.base[y*w + x] = col[bpp-1][getcoli(x, y, p)];
	}

	if((m = allocmemimaged(Rect(0, 0, w, h), RGBA32, &d)) == nil)
		sysfatal("%r");
	if(writememimage(1, m) != 0)
		sysfatal("%r");

	exits(nil);
}
