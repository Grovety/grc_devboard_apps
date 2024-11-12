#include "esp_log.h"
#include "esp_timer.h"

#include "audio_preprocessor.h"
#include "kws_task.h"
#include "nn_model.h"
#include "vad_task.h"

static const char *TAG = "kws_task";

QueueHandle_t xKWSRequestQueue = NULL;
QueueHandle_t xKWSResultQueue = NULL;
EventGroupHandle_t xKWSEventGroup = NULL;

static TaskHandle_t xTaskHandle = NULL;

struct kws_task_param_t {
  nn_model_handle_t model_handle = NULL;
  AudioPreprocessor *pp = NULL;
} static s_kws_task_params;

#define MFCC_BUF_SZ  (KWS_FRAME_NUM * KWS_NUM_MFCC * sizeof(float))
#define FRAME_RATIO  (KWS_FRAME_SHIFT / DET_FRAME_LEN)
#define PROC_BUF_SZ  (KWS_FRAME_SHIFT * MIC_ELEM_BYTES)
#define FBUF_SZ      (KWS_FRAME_LEN * sizeof(float))
#define HALF_FBUF_SZ (KWS_FRAME_SHIFT * sizeof(float))

static const float silence_mfcc_coeffs[KWS_NUM_MFCC] = {
  -247.13936,    8.881784e-16,   2.220446e-14,   -1.0658141e-14,
  8.881784e-16,  -1.5987212e-14, 1.15463195e-14, -4.440892e-15,
  1.0658141e-14, -4.7961635e-14};

void kws_task(void *pv) {
  kws_task_param_t *params = static_cast<kws_task_param_t *>(pv);
  AudioPreprocessor *preprocessor = params->pp;
  nn_model_handle_t model_handle = params->model_handle;
  audio_t proc_buf[KWS_FRAME_SHIFT] = {0};
  float fbuf[KWS_FRAME_LEN] = {0};
  float *half_fbuf = &fbuf[KWS_FRAME_SHIFT];

  xStreamBufferSetTriggerLevel(xWordFramesBuffer, PROC_BUF_SZ);

  for (;;) {
    size_t req_words = 0;
    xQueuePeek(xKWSRequestQueue, &req_words, portMAX_DELAY);

    ESP_LOGD(TAG, "recogninze req_words=%d", req_words);

    float *mfcc_coeffs = new float[KWS_FRAME_NUM * KWS_NUM_MFCC];

    vad_task_start();
    size_t det_words = 0;
    for (; det_words < req_words;) {
      WordDesc_t word = {.frame_num = 0, .max_abs = 0};
      while (xQueueReceive(xWordQueue, &word, pdMS_TO_TICKS(20)) == pdFAIL) {
        if (uxQueueMessagesWaiting(xKWSRequestQueue) == 0) {
          // canceled request
          xQueueReset(xKWSResultQueue);
          break;
        }
      }
      if (word.frame_num == 0) {
        goto CLEANUP;
      } else if (word.frame_num > 0) {
        ESP_LOGD(TAG, "got word: frame_num=%d, max_abs=%d", word.frame_num,
                 word.max_abs);
      }

      memset(proc_buf, 0, PROC_BUF_SZ);
      memset(mfcc_coeffs, 0, MFCC_BUF_SZ);
      memset(fbuf, 0, sizeof(fbuf));

      const int64_t t1 = esp_timer_get_time();
      const size_t mfcc_frames =
        std::min((word.frame_num + FRAME_RATIO - 1) / FRAME_RATIO,
                 size_t(KWS_FRAME_NUM));
      ESP_LOGD(TAG, "mfcc_frames=%d", mfcc_frames);

      size_t frame_idx = 0;

      auto xReceivedBytes =
        xStreamBufferReceive(xWordFramesBuffer, proc_buf, PROC_BUF_SZ, 0);
      ESP_LOGV(TAG, "recv bytes=%d", xReceivedBytes);

      for (size_t i = 0; i < KWS_FRAME_SHIFT; i++) {
        fbuf[i] = static_cast<float>(proc_buf[i]) / word.max_abs;
      }
      frame_idx += FRAME_RATIO;

      size_t proc_frames = 0;
      for (; proc_frames < mfcc_frames;) {
        const auto xReceivedBytes =
          xStreamBufferReceive(xWordFramesBuffer, proc_buf, PROC_BUF_SZ, 0);
        ESP_LOGV(TAG, "recv bytes=%d", xReceivedBytes);

        for (size_t i = 0; i < KWS_FRAME_SHIFT; i++) {
          half_fbuf[i] = static_cast<float>(proc_buf[i]) / word.max_abs;
        }

        preprocessor->MfccCompute(fbuf,
                                  &mfcc_coeffs[proc_frames * KWS_NUM_MFCC]);

        memmove(fbuf, half_fbuf, HALF_FBUF_SZ);
        memset(half_fbuf, 0, HALF_FBUF_SZ);

        if (xReceivedBytes == 0) {
          break;
        } else {
          frame_idx += FRAME_RATIO;
          proc_frames++;
        }
      }

      ESP_LOGD(TAG, "proc %d mic frames", frame_idx);

      memset(proc_buf, 0, PROC_BUF_SZ);
      for (size_t i = mfcc_frames; i < KWS_FRAME_NUM; i++) {
        memcpy(&mfcc_coeffs[i * KWS_NUM_MFCC], silence_mfcc_coeffs,
               KWS_NUM_MFCC * sizeof(float));
      }
      ESP_LOGD(TAG, "preproc %d frames[%d]=%lld us", KWS_FRAME_NUM, det_words,
               esp_timer_get_time() - t1);

      if (word.frame_num > DET_WORD_BUF_FRAME_NUM) {
        xStreamBufferReset(xWordFramesBuffer);
        ESP_LOGW(TAG, "cleared %d frames",
                 word.frame_num - DET_WORD_BUF_FRAME_NUM);
      }

      char result[32] = {0};
      int category = -1;
      nn_model_inference(model_handle, mfcc_coeffs, KWS_FEATURES_LEN,
                         &category);
      nn_model_get_label(model_handle, category, result, sizeof(result));
      ESP_LOGI(TAG, ">> kws[%d]=%s", det_words, result);
      xQueueSend(xKWSResultQueue, &category, 0);
      det_words++;
    }

    ESP_LOGD(TAG, "detected words %d out of %d requested", det_words,
             req_words);

  CLEANUP:
    vad_task_stop();
    xQueueReset(xWordQueue);
    xStreamBufferReset(xWordFramesBuffer);
    delete[] mfcc_coeffs;
    xQueueReceive(xKWSRequestQueue, &req_words, 0);
    xEventGroupSetBits(xKWSEventGroup, KWS_STOPPED_MSK);
  }
}

int kws_task_init(kws_task_conf_t conf) {
  ESP_LOGD(TAG, "KWS_FRAME_LEN=%d, KWS_FRAME_SHIFT=%d, KWS_FRAME_NUM=%d",
           KWS_FRAME_LEN, KWS_FRAME_SHIFT, KWS_FRAME_NUM);
  ESP_LOGD(TAG, "PROC_BUF_SZ=%d, FBUF_SZ=%d, HALF_FBUF_SZ=%d", PROC_BUF_SZ,
           FBUF_SZ, HALF_FBUF_SZ);
  ESP_LOGD(TAG, "FRAME_RATIO=%d, DET_WORD_BUF_FRAME_NUM=%d", FRAME_RATIO,
           DET_WORD_BUF_FRAME_NUM);

  xKWSResultQueue = xQueueCreate(4, sizeof(int));
  if (xKWSResultQueue == NULL) {
    ESP_LOGE(TAG, "Error creating KWS result queue");
    return -1;
  }
  xKWSRequestQueue = xQueueCreate(1, sizeof(size_t));
  if (xKWSRequestQueue == NULL) {
    ESP_LOGE(TAG, "Error creating KWS word queue");
    return -1;
  }
  xKWSEventGroup = xEventGroupCreate();
  if (xKWSEventGroup == NULL) {
    ESP_LOGE(TAG, "Error creating xKWSEventGroup");
    return -1;
  }

  s_kws_task_params.pp = new AudioPreprocessor(
    CONFIG_KWS_SAMPLE_RATE, KWS_NUM_MFCC, KWS_FRAME_LEN, KWS_NUM_FBANK_BINS,
    conf.model_desc->mel_low_freq, conf.model_desc->mel_high_freq);
  s_kws_task_params.model_handle = conf.model_handle;
  auto xReturned =
    xTaskCreate(kws_task, "kws_task", configMINIMAL_STACK_SIZE + 1024 * 6,
                &s_kws_task_params, 1, &xTaskHandle);
  if (xReturned != pdPASS) {
    ESP_LOGE(TAG, "Error creating kws_task");
    return -1;
  }

  xEventGroupSetBits(xKWSEventGroup, KWS_RUNNING_MSK | KWS_STOPPED_MSK);
  return 0;
}

void kws_task_release() {
  xEventGroupClearBits(xKWSEventGroup, KWS_RUNNING_MSK);
  kws_req_cancel();

  if (xTaskHandle) {
    vTaskDelete(xTaskHandle);
    xTaskHandle = NULL;
  }
  if (s_kws_task_params.pp) {
    delete s_kws_task_params.pp;
    s_kws_task_params.pp = NULL;
  }
  s_kws_task_params.model_handle = NULL;

  if (xKWSRequestQueue) {
    vQueueDelete(xKWSRequestQueue);
    xKWSRequestQueue = NULL;
  }
  if (xKWSResultQueue) {
    vQueueDelete(xKWSResultQueue);
    xKWSResultQueue = NULL;
  }
  if (xKWSEventGroup) {
    vEventGroupDelete(xKWSEventGroup);
    xKWSEventGroup = NULL;
  }
}

void kws_req_word(size_t req_words) {
  if (xEventGroupGetBits(xKWSEventGroup) & KWS_RUNNING_MSK) {
    xEventGroupWaitBits(xKWSEventGroup, KWS_STOPPED_MSK, pdTRUE, pdFALSE,
                        portMAX_DELAY);
    if (xQueueSend(xKWSRequestQueue, &req_words, 0) == pdFAIL) {
      ESP_LOGE(TAG, "unable to request kws word");
    }
  }
}

void kws_req_cancel() {
  if (auto req_num = uxQueueMessagesWaiting(xKWSRequestQueue)) {
    ESP_LOGD(TAG, "cancelled %d requests", req_num);
    xQueueReset(xKWSRequestQueue);
    xEventGroupWaitBits(xKWSEventGroup, KWS_STOPPED_MSK, pdFALSE, pdFALSE,
                        portMAX_DELAY);
  }
}
