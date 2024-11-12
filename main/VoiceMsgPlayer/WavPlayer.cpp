#include "WavPlayer.hpp"
#include "I2sTx.hpp"
#include "Types.hpp"
#include "mic_reader.h"

#include "driver/gpio.h"

static const char *TAG = "WavPlayer";

static TaskHandle_t xTaskHandle = NULL;
static audio_t *s_audio_buffer = NULL;
EventGroupHandle_t xWavPlayerEventGroup;
QueueHandle_t xWavPlayerQueue;

static void wp_task(void *pvParameters) {
  for (;;) {
    Sample_t sample;
    xQueuePeek(xWavPlayerQueue, &sample, portMAX_DELAY);
    if (xSemaphoreTake(xMicSema, portMAX_DELAY) == pdPASS) {
      xEventGroupClearBits(xWavPlayerEventGroup, WAV_PLAYER_STOP_MSK);
      while (xQueueReceive(xWavPlayerQueue, &sample, 500) == pdPASS) {
        if (sample.data) {
          const audio_t *audio_data = static_cast<const audio_t *>(sample.data);
          for (size_t j = 0; j < sample.bytes / sizeof(audio_t); j++) {
            s_audio_buffer[j] = float(audio_data[j]) * sample.volume;
          }
          i2s_play_wav(s_audio_buffer, sample.bytes);
        } else {
          ESP_LOGE(TAG, "wav data is not allocated");
        }
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      xSemaphoreGive(xMicSema);
      xEventGroupSetBits(xWavPlayerEventGroup, WAV_PLAYER_STOP_MSK);
    }
  }
}

int initWavPlayer() {
  s_audio_buffer = new audio_t[I2S_TX_SAMPLE_LEN];
  if (!s_audio_buffer) {
    ESP_LOGE(TAG, "Unable to allocate audio_buffer");
    return -1;
  }

  ESP_LOGD(TAG, "Setting up i2s");
  i2s_init();

  xWavPlayerQueue = xQueueCreate(100, sizeof(Sample_t));
  if (xWavPlayerQueue == NULL) {
    ESP_LOGE(TAG, "Error creating wav queue");
    return -1;
  }

  xWavPlayerEventGroup = xEventGroupCreate();
  if (xWavPlayerEventGroup == NULL) {
    ESP_LOGE(TAG, "Error creating xWavPlayerEventGroup");
    return -1;
  }
  xEventGroupSetBits(xWavPlayerEventGroup, WAV_PLAYER_STOP_MSK);

  auto xReturned = xTaskCreate(
    wp_task, "wp_task", configMINIMAL_STACK_SIZE + 1024, NULL, 3, &xTaskHandle);
  if (xReturned != pdPASS) {
    ESP_LOGE(TAG, "Error creating wp_task");
    i2s_release();
    return -1;
  }
  return 0;
}

void releaseWavPlayer() {
  xEventGroupWaitBits(xWavPlayerEventGroup, WAV_PLAYER_STOP_MSK, pdFALSE,
                      pdFALSE, portMAX_DELAY);
  if (xTaskHandle) {
    vTaskDelete(xTaskHandle);
    xTaskHandle = NULL;
  }
  if (s_audio_buffer) {
    delete[] s_audio_buffer;
  }
  if (xWavPlayerQueue) {
    vQueueDelete(xWavPlayerQueue);
    xWavPlayerQueue = NULL;
  }
  if (xWavPlayerEventGroup) {
    vEventGroupDelete(xWavPlayerEventGroup);
    xWavPlayerEventGroup = NULL;
  }
  i2s_release();
}
