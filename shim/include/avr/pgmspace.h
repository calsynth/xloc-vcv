#pragma once
// AVR pgmspace compatibility — flat memory on host.
#include <string.h>
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PGM_P
#define PGM_P const char *
#endif
#define pgm_read_byte(x) (*(const unsigned char *)(x))
#define pgm_read_word(x) (*(const unsigned short *)(x))
#define pgm_read_dword(x) (*(const unsigned long *)(x))
#define pgm_read_float(x) (*(const float *)(x))
#define pgm_read_ptr(x) (*(void *const *)(x))
#define strlen_P strlen
#define memcpy_P memcpy
#define strcpy_P strcpy
#define strcmp_P strcmp
