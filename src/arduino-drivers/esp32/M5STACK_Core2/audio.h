#if defined(ARDUINO_M5STACK_Core2)

#include <M5Core2.h>

typedef void (*audio_callback_t)(int16_t *, size_t);

bool initaudio(audio_callback_t);
void audio_lock();
void audio_unlock();

#endif