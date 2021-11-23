/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

Uint16 file_init(void *filename);
Uint16 file_read(void *dest, Uint16 len);
Uint16 file_write(void *src, Uint16 len, Uint8 flags);
Uint16 file_stat(void *dest, Uint16 len);
Uint16 file_delete(void);
