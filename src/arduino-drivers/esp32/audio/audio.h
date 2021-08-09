#if defined(ESP32)

#include <Arduino.h>

typedef void (*audio_callback_t)(int16_t *, size_t);

bool initaudio(audio_callback_t);
void audio_lock();
void audio_unlock();

#endif