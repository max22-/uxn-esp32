#if defined(ESP32)

#include <Arduino.h>
#include <driver/i2s.h>
#include "audio.h"
#include "../../../esp32-config.h"

extern "C" {
#include "uxn.h"
#include "devices/apu.h"
}

void audio_task(void *params);

static SemaphoreHandle_t mutex;

static const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_FREQUENCY,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0, // default interrupt priority
	#warning I should maybe improve the following 2 lines (but it works ^^)
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = true
};

static const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_DATA_OUT_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
};

bool
initaudio(audio_callback_t callback)
{
	i2s_driver_uninstall(I2S_NUM_0);

	if(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK) {
		fprintf(stderr, "error : i2s_driver_install\n");
		return false;
	}

	if(i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK) {
		fprintf(stderr, "error : i2s_set_pin\n");
		return false;
	}

	mutex = xSemaphoreCreateMutex();
	xTaskCreate(audio_task, "audio_task", 2000, (void *)callback, 10, nullptr);

	return true;
}

void
audio_lock()
{
	xSemaphoreTake(mutex, portMAX_DELAY);
}

void
audio_unlock()
{
	xSemaphoreGive(mutex);
}

void
audio_task(void *params)
{
	audio_callback_t callback = (audio_callback_t)params;
	size_t bytes_written = 0;
	const int samples = 64;
	int16_t buffer[samples * 2];

	for(;;) {
		xSemaphoreTake(mutex, portMAX_DELAY);
		callback(buffer, sizeof(buffer));
		xSemaphoreGive(mutex);
		i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &bytes_written, portMAX_DELAY);
	}
}

#endif