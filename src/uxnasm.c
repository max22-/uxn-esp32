#include <stdio.h>

/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define TRIM 0x0100

typedef unsigned char Uint8;
typedef signed char Sint8;
typedef unsigned short Uint16;

typedef struct {
	char name[64], items[256][64];
	Uint8 len;
	Uint16 refs;
} Macro;

typedef struct {
	char name[64];
	Uint16 addr, refs;
} Label;

typedef struct {
	Uint8 data[256 * 256], mlen;
	Uint16 ptr, length, llen;
	Label labels[512];
	Macro macros[256];
} Program;

Program p;

/* clang-format off */

static char ops[][4] = {
	"BRK", "LIT", "NOP", "POP", "DUP", "SWP", "OVR", "ROT",
	"EQU", "NEQ", "GTH", "LTH", "JMP", "JCN", "JSR", "STH",
	"LDZ", "STZ", "LDR", "STR", "LDA", "STA", "DEI", "DEO",
	"ADD", "SUB", "MUL", "DIV", "AND", "ORA", "EOR", "SFT"
};

static int   scmp(char *a, char *b, int len) { int i = 0; while(a[i] == b[i] && i < len) if(!a[i++]) return 1; return 0; } /* string compare */
static int   sihx(char *s) { int i = 0; char c; while((c = s[i++])) if(!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f')) return 0; return i > 1; } /* string is hexadecimal */
static int   shex(char *s) { int n = 0, i = 0; char c; while((c = s[i++])) if(c >= '0' && c <= '9') n = n * 16 + (c - '0'); else if(c >= 'a' && c <= 'f') n = n * 16 + 10 + (c - 'a'); return n; } /* string to num */
static int   slen(char *s) { int i = 0; while(s[i] && s[++i]) ; return i; } /* string length */
static char *scpy(char *src, char *dst, int len) { int i = 0; while((dst[i] = src[i]) && i < len - 2) i++; dst[i + 1] = '\0'; return dst; } /* string copy */
static char *scat(char *dst, const char *src) { char *ptr = dst + slen(dst); while(*src) *ptr++ = *src++; *ptr = '\0'; return dst; } /* string cat */

#pragma mark - Helpers

/* clang-format on */

#pragma mark - I/O

static void
pushbyte(Uint8 b, int lit)
{
	if(lit) pushbyte(0x01, 0);
	p.data[p.ptr++] = b;
	p.length = p.ptr;
}

static void
pushshort(Uint16 s, int lit)
{
	if(lit) pushbyte(0x21, 0);
	pushbyte((s >> 8) & 0xff, 0);
	pushbyte(s & 0xff, 0);
}

static void
pushword(char *s)
{
	int i = 0;
	char c;
	while((c = s[i++])) pushbyte(c, 0);
}

static Macro *
findmacro(char *name)
{
	int i;
	for(i = 0; i < p.mlen; ++i)
		if(scmp(p.macros[i].name, name, 64))
			return &p.macros[i];
	return NULL;
}

static Label *
findlabel(char *name)
{
	int i;
	for(i = 0; i < p.llen; ++i)
		if(scmp(p.labels[i].name, name, 64))
			return &p.labels[i];
	return NULL;
}

static Uint8
findopcode(char *s)
{
	int i;
	for(i = 0; i < 0x20; ++i) {
		int m = 0;
		char *o = ops[i];
		if(o[0] != s[0] || o[1] != s[1] || o[2] != s[2])
			continue;
		while(s[3 + m]) {
			if(s[3 + m] == '2')
				i |= (1 << 5); /* mode: short */
			else if(s[3 + m] == 'r')
				i |= (1 << 6); /* mode: return */
			else if(s[3 + m] == 'k')
				i |= (1 << 7); /* mode: keep */
			else
				return 0; /* failed to match */
			m++;
		}
		return i;
	}
	return 0;
}

static char *
sublabel(char *src, char *scope, char *name)
{
	return scat(scat(scpy(scope, src, 64), "/"), name);
}

#pragma mark - Parser

static int
error(char *name, char *msg)
{
	fprintf(stderr, "%s: %s\n", name, msg);
	return 0;
}

static int
makemacro(char *name, FILE *f)
{
	Macro *m;
	char word[64];
	if(findmacro(name))
		return error("Macro duplicate", name);
	if(sihx(name) && slen(name) % 2 == 0)
		return error("Macro name is hex number", name);
	if(findopcode(name) || !slen(name))
		return error("Macro name is invalid", name);
	m = &p.macros[p.mlen++];
	scpy(name, m->name, 64);
	while(fscanf(f, "%s", word)) {
		if(word[0] == '{') continue;
		if(word[0] == '}') break;
		if(m->len > 64)
			return error("Macro too large", name);
		if(slen(word) >= 64)
			return error("Word too long", name);
		scpy(word, m->items[m->len++], 64);
	}
	return 1;
}

static int
makelabel(char *name, Uint16 addr)
{
	Label *l;
	if(findlabel(name))
		return error("Label duplicate", name);
	if(sihx(name) && slen(name) % 2 == 0)
		return error("Label name is hex number", name);
	if(findopcode(name) || !slen(name))
		return error("Label name is invalid", name);
	l = &p.labels[p.llen++];
	l->addr = addr;
	l->refs = 0;
	scpy(name, l->name, 64);
	return 1;
}

static int
skipblock(char *w, int *cap, char a, char b)
{
	if(w[0] == b) {
		*cap = 0;
		return 1;
	}
	if(w[0] == a) *cap = 1;
	if(*cap) return 1;
	return 0;
}

static int
walktoken(char *w)
{
	Macro *m;
	if(findopcode(w) || scmp(w, "BRK", 4))
		return 1;
	switch(w[0]) {
	case '[': return 0;
	case ']': return 0;
	case '\'': return 1;
	case '.': return 2; /* zero-page: LIT addr-lb */
	case ',': return 2; /* relative:  LIT addr-rel */
	case ':': return 2; /* absolute:      addr-hb addr-lb */
	case ';': return 3; /* absolute:  LIT addr-hb addr-lb */
	case '$': return shex(w + 1);
	case '#': return slen(w + 1) == 4 ? 3 : 2;
	case '"': return slen(w + 1);
	}
	if((m = findmacro(w))) {
		int i, res = 0;
		for(i = 0; i < m->len; ++i)
			res += walktoken(m->items[i]);
		return res;
	}
	return error("Invalid token", w);
}

static int
parsetoken(char *w)
{
	Label *l;
	Macro *m;
	if(w[0] == '.' && (l = findlabel(w + 1))) { /* zero-page */
		if(l->addr > 0xff)
			return error("Address is not in zero page", w);
		pushbyte(l->addr, 1);
		return ++l->refs;
	} else if(w[0] == ',' && (l = findlabel(w + 1))) { /* relative */
		int off = l->addr - p.ptr - 3;
		if(off < -126 || off > 126)
			return error("Address is too far", w);
		pushbyte((Sint8)off, 1);
		return ++l->refs;
	} else if(w[0] == ':' && (l = findlabel(w + 1))) { /* raw */
		pushshort(l->addr, 0);
		return ++l->refs;
	} else if(w[0] == ';' && (l = findlabel(w + 1))) { /* absolute */
		pushshort(l->addr, 1);
		return ++l->refs;
	} else if(findopcode(w) || scmp(w, "BRK", 4)) { /* opcode */
		pushbyte(findopcode(w), 0);
		return 1;
	} else if(w[0] == '"') { /* string */
		pushword(w + 1);
		return 1;
	} else if(w[0] == '\'') { /* char */
		pushbyte((Uint8)w[1], 0);
		return 1;
	} else if(w[0] == '#') { /* immediate */
		if(slen(w + 1) == 1)
			pushbyte((Uint8)w[1], 1);
		if(sihx(w + 1) && slen(w + 1) == 2)
			pushbyte(shex(w + 1), 1);
		else if(sihx(w + 1) && slen(w + 1) == 4)
			pushshort(shex(w + 1), 1);
		else
			return error("Invalid hexadecimal literal", w);
		return 1;
	} else if(sihx(w)) { /* raw */
		if(slen(w) == 2)
			pushbyte(shex(w), 0);
		else if(slen(w) == 4)
			pushshort(shex(w), 0);
		else
			return error("Invalid hexadecimal value", w);
		return 1;
	} else if((m = findmacro(w))) {
		int i;
		for(i = 0; i < m->len; ++i)
			if(!parsetoken(m->items[i]))
				return error("Invalid macro", m->name);
		return ++m->refs;
	}
	return error("Invalid token", w);
}

static int
pass1(FILE *f)
{
	int ccmnt = 0;
	Uint16 addr = 0;
	char w[64], scope[64], subw[64];
	while(fscanf(f, "%s", w) == 1) {
		if(skipblock(w, &ccmnt, '(', ')')) continue;
		if(w[0] == '|') {
			if(!sihx(w + 1))
				return error("Pass 1 - Invalid padding", w);
			addr = shex(w + 1);
		} else if(w[0] == '%') {
			if(!makemacro(w + 1, f))
				return error("Pass 1 - Invalid macro", w);
		} else if(w[0] == '@') {
			if(!makelabel(w + 1, addr))
				return error("Pass 1 - Invalid label", w);
			scpy(w + 1, scope, 64);
		} else if(w[0] == '&') {
			if(!makelabel(sublabel(subw, scope, w + 1), addr))
				return error("Pass 1 - Invalid sublabel", w);
		} else if(sihx(w))
			addr += slen(w) / 2;
		else
			addr += walktoken(w);
	}
	rewind(f);
	return 1;
}

static int
pass2(FILE *f)
{
	int ccmnt = 0, cmacr = 0;
	char w[64], scope[64], subw[64];
	while(fscanf(f, "%s", w) == 1) {
		if(w[0] == '%') continue;
		if(w[0] == '&') continue;
		if(w[0] == '[') continue;
		if(w[0] == ']') continue;
		if(skipblock(w, &ccmnt, '(', ')')) continue;
		if(skipblock(w, &cmacr, '{', '}')) continue;
		if(w[0] == '|') {
			if(p.length && shex(w + 1) < p.ptr)
				return error("Pass 2 - Memory overwrite", w);
			p.ptr = shex(w + 1);
			continue;
		} else if(w[0] == '$') {
			p.ptr += shex(w + 1);
			continue;
		} else if(w[0] == '@') {
			scpy(w + 1, scope, 64);
			continue;
		}
		if(w[1] == '&')
			scpy(sublabel(subw, scope, w + 2), w + 1, 64);
		if(!parsetoken(w))
			return error("Pass 2 - Unknown label", w);
	}
	return 1;
}

static void
cleanup(char *filename)
{
	int i;
	for(i = 0; i < p.llen; ++i)
		if(p.labels[i].name[0] >= 'A' && p.labels[i].name[0] <= 'Z')
			continue; /* Ignore capitalized labels(devices) */
		else if(!p.labels[i].refs)
			fprintf(stderr, "--- Unused label: %s\n", p.labels[i].name);
	for(i = 0; i < p.mlen; ++i)
		if(!p.macros[i].refs)
			fprintf(stderr, "--- Unused macro: %s\n", p.macros[i].name);
	printf("Assembled %s(%d bytes), %d labels, %d macros.\n", filename, (p.length - TRIM), p.llen, p.mlen);
}

int
main(int argc, char *argv[])
{
	FILE *f;
	if(argc < 3)
		return !error("usage", "input.tal output.rom");
	if(!(f = fopen(argv[1], "r")))
		return !error("Load", "Failed to open source.");
	if(!pass1(f) || !pass2(f))
		return !error("Assembly", "Failed to assemble rom.");
	fwrite(p.data + TRIM, p.length - TRIM, 1, fopen(argv[2], "wb"));
	fclose(f);
	cleanup(argv[2]);
	return 0;
}
