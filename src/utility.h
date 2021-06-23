#ifdef ARDUINO

#ifdef BOARD_HAS_PSRAM
	#define ext_malloc ps_malloc
#else
	#define ext_malloc malloc
#endif

#endif