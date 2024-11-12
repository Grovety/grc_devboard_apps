#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "string.h"

#include <driver/i2s_pdm.h>
#include <driver/i2s_types.h>

#include "def.h"
#include "i2s_rx_slot.h"

static const char *TAG = "i2s_rx_slot";

static i2s_chan_handle_t s_rx_handle = NULL;

static size_t s_rx_queue_ovf_count = 0;
static IRAM_ATTR bool i2s_rx_queue_overflow_callback(i2s_chan_handle_t handle,
                                                     i2s_event_data_t *event,
                                                     void *data) {
  s_rx_queue_ovf_count++;
  return false;
}

void i2s_rx_slot_init(const rx_slot_conf_t &conf) {
  i2s_pdm_slot_mask_t i2s_slot_mask = I2S_PDM_SLOT_RIGHT;
  switch (conf.slot_type) {
  case stLeft:
    i2s_slot_mask = I2S_PDM_SLOT_LEFT;
    break;
  case stRight:
    i2s_slot_mask = I2S_PDM_SLOT_RIGHT;
    break;
  case stBoth:
    i2s_slot_mask = I2S_PDM_SLOT_BOTH;
    break;
  default:
    break;
  }

  i2s_chan_config_t chan_cfg =
    I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num = I2S_RX_DMA_BUF_NUM;
  chan_cfg.dma_frame_num = I2S_RX_DMA_BUF_LEN;
  chan_cfg.auto_clear = true;

  i2s_pdm_rx_clk_config_t clk_cfg = {
    .sample_rate_hz = conf.sample_rate,
    .clk_src = I2S_CLK_SRC_DEFAULT,
    .mclk_multiple = I2S_MCLK_MULTIPLE_128,
  };

  i2s_pdm_rx_config_t rx_pdm_rx_cfg = {
    .clk_cfg = clk_cfg,
    .slot_cfg =
      I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_RX_BIT_WIDTH, I2S_RX_SLOT_MODE),
    .gpio_cfg =
      {
        .clk = MIC_WS_PIN,
        .din = MIC_DATA_PIN,
        .invert_flags =
          {
            .clk_inv = false,
          },
      },
  };

  rx_pdm_rx_cfg.slot_cfg.slot_mask = i2s_slot_mask;

  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &s_rx_handle));
  ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(s_rx_handle, &rx_pdm_rx_cfg));

  i2s_event_callbacks_t cbs = {
    .on_recv = NULL,
    .on_recv_q_ovf = i2s_rx_queue_overflow_callback,
    .on_sent = NULL,
    .on_send_q_ovf = NULL,
  };
  ESP_ERROR_CHECK(i2s_channel_register_event_callback(s_rx_handle, &cbs, NULL));
}

void i2s_rx_slot_start() { ESP_ERROR_CHECK(i2s_channel_enable(s_rx_handle)); }

void i2s_rx_slot_stop() { ESP_ERROR_CHECK(i2s_channel_disable(s_rx_handle)); }

void i2s_rx_slot_release() {
  if (s_rx_handle) {
    ESP_ERROR_CHECK(i2s_del_channel(s_rx_handle));
    s_rx_handle = NULL;
  }
}

int i2s_rx_slot_read(void *buffer, size_t bytes, size_t timeout_ms) {
  size_t read = 0;
  esp_err_t ret =
    i2s_channel_read(s_rx_handle, buffer, bytes, &read, timeout_ms);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "failed to receive item, %s", esp_err_to_name(ret));
    return -1;
  }
  if (read != bytes) {
    ESP_LOGE(TAG, "read %u/%u bytes", read, bytes);
    return -1;
  }
  return 0;
};
