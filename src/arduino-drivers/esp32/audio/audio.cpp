#if defined(ESP32)

#include <Arduino.h>
#include <driver/i2s.h>
#include "audio.h"

#define CONFIG_I2S_BCK_PIN 12
#define CONFIG_I2S_LRCK_PIN 0
#define CONFIG_I2S_DATA_PIN 2
#define CONFIG_I2S_DATA_IN_PIN 34

#define Speak_I2S_NUMBER I2S_NUM_0

void audio_task(void *params);

static SemaphoreHandle_t mutex;

bool
initaudio(audio_callback_t callback)
{
	esp_err_t err = ESP_OK;

	i2s_driver_uninstall(Speak_I2S_NUMBER);
	i2s_config_t i2s_config = {
		.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
		.sample_rate = 44100,
		.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
		.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
		.communication_format = I2S_COMM_FORMAT_I2S,
		.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
		.dma_buf_count = 2,
		.dma_buf_len = 128,
		.use_apll = false,
		.tx_desc_auto_clear = true,
	};

	err += i2s_driver_install(Speak_I2S_NUMBER, &i2s_config, 0, NULL);
	i2s_pin_config_t tx_pin_config;

	tx_pin_config.bck_io_num = CONFIG_I2S_BCK_PIN;
	tx_pin_config.ws_io_num = CONFIG_I2S_LRCK_PIN;
	tx_pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
	tx_pin_config.data_in_num = CONFIG_I2S_DATA_IN_PIN;
	err += i2s_set_pin(Speak_I2S_NUMBER, &tx_pin_config);
	err += i2s_set_clk(Speak_I2S_NUMBER, 44100, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

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
	const int samples = 128;
	int16_t stereo_buffer[samples * 2];

	for(;;) {

		xSemaphoreTake(mutex, portMAX_DELAY);
		callback(stereo_buffer, sizeof(stereo_buffer));
		xSemaphoreGive(mutex);

		int16_t mono_buffer[samples];
		for(int i = 0; i < samples; i++)
			mono_buffer[i] = (stereo_buffer[2 * i] + stereo_buffer[2 * i + 1]);

		i2s_write(Speak_I2S_NUMBER, mono_buffer, sizeof(mono_buffer), &bytes_written, portMAX_DELAY);
	}
}

#endif