#include "def.h"
#include "freertos/FreeRTOS.h"

#include <algorithm>
#include <cmath>

#include "esp_log.h"

#include "i2s_rx_slot.h"
#include "mic_proc.h"
#include "mic_reader.h"

static const char *TAG = "mic_reader";

#define FILTER_INIT_FRAME_NUM 100
#define MIC_TEST_FRAME_NUM    10

static constexpr size_t DEF_MEAN = 1 << (8 * MIC_ELEM_BYTES - 6);
static constexpr size_t DEF_STD_DEV =
  1 << (8 * MIC_ELEM_BYTES - 3); // 1/4 of dynamic range

SemaphoreHandle_t xMicSema = NULL;

static dc_blocker<int32_t> s_dc_blocker;

static audio_t mic_frame_buf[MIC_FRAME_LEN * MIC_CHANNEL_NUM] = {0};

int mic_reader_read_frame(audio_t *buffer, size_t timeout_ms) {
  if (i2s_rx_slot_read(mic_frame_buf, MIC_FRAME_SZ, timeout_ms) < 0) {
    return -1;
  }

  for (size_t i = 0; i < MIC_FRAME_LEN; i++) {
    const audio_t val =
#if CONFIG_MIC_CHANNEL_BOTH
      mic_frame_buf[i * 2] + mic_frame_buf[i * 2 + 1];
#else
      mic_frame_buf[i];
#endif
    buffer[i] = s_dc_blocker.proc_val(val);
  }
  return 0;
};

static float compute_mean(const audio_t *data, size_t samples) {
  int32_t sum = 0;
  for (size_t i = 0; i < samples; i++) {
    sum += data[i];
  }
  return float(sum) / samples;
}

static float compute_std_dev(const audio_t *data, size_t samples,
                             float mean_val) {
  float sum_sq = 0;
  for (size_t i = 0; i < samples; i++) {
    sum_sq += std::pow(data[i] - mean_val, 2);
  }
  return std::sqrt(sum_sq / float(samples));
}

static MicResult_t test_microphone() {
  float mean = 0.0;
  float std_dev = 0.0;

  for (size_t i = 0; i < MIC_TEST_FRAME_NUM; i++) {
    memset(mic_frame_buf, 0, MIC_FRAME_SZ);
    mic_reader_read_frame(mic_frame_buf, MIC_FRAME_LEN_MS * 2);
    const float frame_mean = compute_mean(mic_frame_buf, MIC_FRAME_LEN);
    const float frame_std_dev =
      compute_std_dev(mic_frame_buf, MIC_FRAME_LEN, frame_mean);
    ESP_LOGV(TAG, "frame_mean=%f, frame_std_dev=%f", mean, std_dev);
    mean += frame_mean / MIC_TEST_FRAME_NUM;
    std_dev += frame_std_dev / MIC_TEST_FRAME_NUM;
  }

  ESP_LOGD(TAG, "mean=%f, std_dev=%f", mean, std_dev);
  if (mean == 0.f && std_dev == 0.f) {
    return MIC_SILENT;
  } else if (abs(mean) > DEF_MEAN || std_dev > DEF_STD_DEV) {
    return MIC_NOISY;
  } else {
    return MIC_OK;
  }
}

MicResult_t mic_reader_init() {
  xMicSema = xSemaphoreCreateBinary();
  if (xMicSema) {
    xSemaphoreGive(xMicSema);
  } else {
    ESP_LOGE(TAG, "Error creating xMicSema");
    return MIC_INIT_ERROR;
  }

  const rx_slot_conf_t rx_slot_conf[] = {
#if CONFIG_MIC_CHANNEL_BOTH
    {
      .sample_rate = CONFIG_MIC_SAMPLE_RATE,
      .slot_type = stBoth,
    },
#else
    {
      .sample_rate = CONFIG_MIC_SAMPLE_RATE,
      .slot_type = stRight,
    },
    {
      .sample_rate = CONFIG_MIC_SAMPLE_RATE,
      .slot_type = stLeft,
    },
#endif
  };

  ESP_LOGD(TAG, "DEF_MEAN=%d, DEF_STD_DEV=%d", DEF_MEAN, DEF_STD_DEV);
  MicResult_t test_result;
  for (size_t i = 0; i < sizeof(rx_slot_conf) / sizeof(rx_slot_conf[0]); ++i) {
    ESP_LOGD(TAG, "Try conf[%d]", i);
    i2s_rx_slot_init(rx_slot_conf[i]);
    i2s_rx_slot_start();

    // read first bad samples, init filter
    for (size_t i = 0; i < FILTER_INIT_FRAME_NUM; i++) {
      mic_reader_read_frame(mic_frame_buf, MIC_FRAME_LEN_MS * 2);
    }

    test_result = test_microphone();
    if (test_result == MIC_OK) {
      break;
    } else {
      i2s_rx_slot_stop();
      i2s_rx_slot_release();
    }
  }
  if (test_result != MIC_OK) {
    return test_result;
  }

  return MIC_OK;
}

void mic_reader_release() {
  i2s_rx_slot_stop();
  i2s_rx_slot_release();

  if (xMicSema) {
    vSemaphoreDelete(xMicSema);
    xMicSema = NULL;
  }
}
