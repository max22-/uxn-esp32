#ifndef UTILITY_H
#define UTILITY_H

#if defined(ESP32) && defined(BOARD_HAS_PSRAM)
	#define ext_malloc ps_malloc
#else
	#define ext_malloc malloc
#endif

#endif