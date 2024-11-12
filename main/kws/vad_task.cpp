#include "def.h"
#include "freertos/FreeRTOS.h"

#include <algorithm>
#include <cmath>

#include "esp_agc.h"
#include "esp_dsp.h"
#include "esp_log.h"
#include "esp_ns.h"
#include "esp_vad.h"

#include "i2s_rx_slot.h"
#include "mic_proc.h"
#include "mic_reader.h"
#include "vad_task.h"

static const char *TAG = "vad_task";

#define AGC_FRAME_LEN_MS 10
#define AGC_FRAME_LEN    (CONFIG_MIC_SAMPLE_RATE / 1000 * AGC_FRAME_LEN_MS)

static TaskHandle_t xTaskHandle = NULL;
static ns_handle_t s_ns_handle = NULL;
static void *s_agc_handle = NULL;
static vad_handle_t s_vad_handle = NULL;

QueueHandle_t xWordQueue = NULL;
StreamBufferHandle_t xWordFramesBuffer = NULL;
EventGroupHandle_t xVADEventGroup = NULL;

static audio_t current_frames[DET_VOICED_FRAMES_WINDOW * DET_FRAME_LEN] = {0};

static void vad_task(void *pv) {
  size_t max_abs_arr[DET_VOICED_FRAMES_WINDOW] = {0};
  uint8_t is_speech_arr[DET_VOICED_FRAMES_WINDOW] = {0};
  size_t cur_frame = 0;
  size_t num_voiced = 0;
  uint8_t trig = 0;
  WordDesc_t word;

  for (;;) {
    const auto xBits =
      xEventGroupWaitBits(xVADEventGroup, VAD_RUNNING_MSK | VAD_STOP_MSK,
                          pdFALSE, pdFALSE, portMAX_DELAY);

    if (xBits & VAD_STOP_MSK) {
      break;
    } else if (!(xBits & VAD_RUNNING_MSK)) {
      continue;
    }

    audio_t *proc_frame =
      &current_frames[(cur_frame % DET_VOICED_FRAMES_WINDOW) * DET_FRAME_LEN];

    if (mic_reader_read_frame(proc_frame, MIC_FRAME_LEN_MS) < 0) {
      continue;
    }

    esp_agc_process(s_agc_handle, proc_frame, proc_frame, AGC_FRAME_LEN,
                    CONFIG_MIC_SAMPLE_RATE);

    ns_process(s_ns_handle, proc_frame, proc_frame);

    const size_t max_abs = compute_max_abs(proc_frame, DET_FRAME_LEN);

    max_abs_arr[cur_frame % DET_VOICED_FRAMES_WINDOW] = max_abs;

    const auto vad_res = vad_process(s_vad_handle, proc_frame,
                                     CONFIG_MIC_SAMPLE_RATE, AGC_FRAME_LEN_MS);
    const uint8_t is_speech = vad_res == VAD_SPEECH;

    num_voiced += is_speech;
    is_speech_arr[cur_frame % DET_VOICED_FRAMES_WINDOW] = is_speech;

    if (!trig) {
      if (num_voiced >= DET_VOICED_FRAMES_THRESHOLD) {
        word.frame_num = 0;
        word.max_abs = 0;
        trig = 1;
        ESP_LOGD(TAG, "__start[%d]=%d, max_abs=%d",
                 cur_frame - DET_VOICED_FRAMES_WINDOW, cur_frame, max_abs);
        for (size_t k = 1; k <= DET_VOICED_FRAMES_WINDOW; k++) {
          const size_t frame_num = (cur_frame + k) % DET_VOICED_FRAMES_WINDOW;
          audio_t *frame_ptr = &current_frames[frame_num * DET_FRAME_LEN];
          const auto xBytesSent =
            xStreamBufferSend(xWordFramesBuffer, frame_ptr, DET_FRAME_SZ, 0);
          if (xBytesSent < DET_FRAME_SZ) {
            ESP_LOGW(TAG, "xWordFramesBuffer: xBytesSent=%d (%d)", xBytesSent,
                     DET_FRAME_SZ);
          }
          word.frame_num++;
          word.max_abs = std::max(word.max_abs, max_abs_arr[frame_num]);
        }
      }
    } else {
      if (num_voiced <= DET_UNVOICED_FRAMES_THRESHOLD) {
        trig = 0;
        ESP_LOGD(TAG, "__end[%d]=%d, max_abs=%d",
                 cur_frame - DET_VOICED_FRAMES_WINDOW, cur_frame, max_abs);
        xQueueSend(xWordQueue, &word, 0);
      } else {
        word.frame_num++;
        word.max_abs = std::max(word.max_abs, max_abs);
        const auto xBytesSent =
          xStreamBufferSend(xWordFramesBuffer, proc_frame, DET_FRAME_SZ, 0);
        if (xBytesSent < DET_FRAME_SZ) {
          ESP_LOGW(TAG, "xWordFramesBuffer: xBytesSent=%d (%d)", xBytesSent,
                   DET_FRAME_SZ);
        }
      }
    }

    num_voiced -= is_speech_arr[(cur_frame + 1) % DET_VOICED_FRAMES_WINDOW];

    cur_frame++;
  }

  ESP_LOGD(TAG, "stop vad_task");
  xEventGroupSetBits(xVADEventGroup, VAD_STOPPED_MSK);
  xTaskHandle = NULL;
  vTaskDelete(NULL);
}

int vad_task_init() {
  ESP_LOGD(TAG, "MIC_FRAME_LEN=%d, MIC_FRAME_SZ=%d", MIC_FRAME_LEN,
           MIC_FRAME_SZ);
  ESP_LOGD(TAG, "DET_FRAME_LEN=%d, DET_FRAME_SZ=%d", DET_FRAME_LEN,
           DET_FRAME_SZ);
  ESP_LOGD(TAG, "DET_WORD_BUF_FRAME_NUM=%d, DET_WORD_BUF_SZ=%d",
           DET_WORD_BUF_FRAME_NUM, DET_WORD_BUF_SZ);

  xVADEventGroup = xEventGroupCreate();
  if (xVADEventGroup == NULL) {
    ESP_LOGE(TAG, "Error creating xVADEventGroup");
    return -1;
  }

  s_agc_handle = esp_agc_open(3, CONFIG_MIC_SAMPLE_RATE);
  if (!s_agc_handle) {
    ESP_LOGE(TAG, "Unable to create agc");
    return -1;
  }
  set_agc_config(s_agc_handle, CONFIG_MIC_GAIN, 1, 0);

  s_ns_handle = ns_pro_create(MIC_FRAME_LEN_MS, 2, CONFIG_KWS_SAMPLE_RATE);
  if (!s_ns_handle) {
    ESP_LOGE(TAG, "Unable to create esp_ns");
    return -1;
  }

  s_vad_handle = vad_create(VAD_MODE_4);
  if (!s_vad_handle) {
    ESP_LOGE(TAG, "Unable to create esp_vad");
    return -1;
  }

  xWordQueue = xQueueCreate(DET_MAX_WORDS, sizeof(WordDesc_t));
  if (xWordQueue == NULL) {
    ESP_LOGE(TAG, "Error creating word queue");
    return -1;
  }
  xWordFramesBuffer = xStreamBufferCreate(DET_WORD_BUF_SZ, DET_FRAME_SZ);
  if (xWordFramesBuffer == NULL) {
    ESP_LOGE(TAG, "Error creating word stream buffer");
    return -1;
  }

  auto xReturned =
    xTaskCreate(vad_task, "vad_task", configMINIMAL_STACK_SIZE + 1024 * 8, NULL,
                2, &xTaskHandle);
  if (xReturned != pdPASS) {
    ESP_LOGE(TAG, "Error creating vad_task");
    return -1;
  }

  return 0;
}

void vad_task_release() {
  xEventGroupSetBits(xVADEventGroup, VAD_STOP_MSK);
  xEventGroupWaitBits(xVADEventGroup, VAD_STOPPED_MSK, pdFALSE, pdFALSE,
                      portMAX_DELAY);

  if (xTaskHandle) {
    vTaskDelete(xTaskHandle);
    xTaskHandle = NULL;
  }

  if (xVADEventGroup) {
    vEventGroupDelete(xVADEventGroup);
    xVADEventGroup = NULL;
  }

  if (s_agc_handle) {
    esp_agc_close(s_agc_handle);
    s_agc_handle = NULL;
  }
  if (s_ns_handle) {
    ns_destroy(s_ns_handle);
    s_ns_handle = NULL;
  }
  if (s_vad_handle) {
    vad_destroy(s_vad_handle);
    s_vad_handle = NULL;
  }

  if (xWordQueue) {
    vQueueDelete(xWordQueue);
    xWordQueue = NULL;
  }
  if (xWordFramesBuffer) {
    vStreamBufferDelete(xWordFramesBuffer);
    xWordFramesBuffer = NULL;
  }
}

void vad_task_start() {
  xSemaphoreTake(xMicSema, portMAX_DELAY);
  xEventGroupSetBits(xVADEventGroup, VAD_RUNNING_MSK);
}

void vad_task_stop() {
  xEventGroupClearBits(xVADEventGroup, VAD_RUNNING_MSK);
  xSemaphoreGive(xMicSema);
}
