#include "../uxn.h"
#include "file.h"

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static FILE *f;
static DIR *d;
static char *current_filename = "";
static enum { IDLE,
	FILE_READ,
	FILE_WRITE,
	DIR_READ } state;
static struct dirent *de;

static void
reset(void)
{
	if(f != NULL) {
		fclose(f);
		f = NULL;
	}
	if(d != NULL) {
		closedir(d);
		d = NULL;
	}
	de = NULL;
	state = IDLE;
}

static Uint16
get_entry(char *p, Uint16 len, const char *pathname, const char *basename, int fail_nonzero)
{
	struct stat st;
	if(len < strlen(basename) + 7)
		return 0;
	if(stat(pathname, &st))
		return fail_nonzero ? snprintf(p, len, "!!!! %s\n", basename) : 0;
	else if(S_ISDIR(st.st_mode))
		return snprintf(p, len, "---- %s\n", basename);
	else if(st.st_size < 0x10000)
		return snprintf(p, len, "%04x %s\n", (Uint16)st.st_size, basename);
	else
		return snprintf(p, len, "???? %s\n", basename);
}

static Uint16
file_read_dir(char *dest, Uint16 len)
{
	static char pathname[4096];
	char *p = dest;
	if(de == NULL) de = readdir(d);
	for(; de != NULL; de = readdir(d)) {
		Uint16 n;
		if(de->d_name[0] == '.' && de->d_name[1] == '\0')
			continue;
		snprintf(pathname, sizeof(pathname), "%s/%s", current_filename, de->d_name);
		n = get_entry(p, len, pathname, de->d_name, 1);
		if(!n) break;
		p += n;
		len -= n;
	}
	return p - dest;
}

Uint16
file_init(void *filename)
{
	reset();
	current_filename = filename;
	return 0;
}

Uint16
file_read(void *dest, Uint16 len)
{
	if(state != FILE_READ && state != DIR_READ) {
		reset();
		if((d = opendir(current_filename)) != NULL)
			state = DIR_READ;
		else if((f = fopen(current_filename, "rb")) != NULL)
			state = FILE_READ;
	}
	if(state == FILE_READ)
		return fread(dest, 1, len, f);
	if(state == DIR_READ)
		return file_read_dir(dest, len);
	return 0;
}

Uint16
file_write(void *src, Uint16 len, Uint8 flags)
{
	Uint16 ret = 0;
	if(state != FILE_WRITE) {
		reset();
		if((f = fopen(current_filename, (flags & 0x01) ? "ab" : "wb")) != NULL)
			state = FILE_WRITE;
	}
	if(state == FILE_WRITE) {
		if((ret = fwrite(src, 1, len, f)) > 0 && fflush(f) != 0)
			ret = 0;
	}
	return ret;
}

Uint16
file_stat(void *dest, Uint16 len)
{
	char *basename = strrchr(current_filename, '/');
	if(basename != NULL)
		++basename;
	else
		basename = current_filename;
	return get_entry(dest, len, current_filename, basename, 0);
}

Uint16
file_delete(void)
{
	return unlink(current_filename);
}
