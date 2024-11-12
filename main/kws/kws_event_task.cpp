#include "Event.hpp"

#include "kws_event_task.h"
#include "kws_task.h"
#include "nn_model.h"
#include "portmacro.h"

static const char *TAG = "KWS";

static TaskHandle_t xTaskHandle = NULL;
static kws_event_cb_t s_event_cb = NULL;

int kws_event_task_init(kws_event_cb_t event_cb) {
  auto xReturned =
    xTaskCreate(event_cb, "kws_event_task", configMINIMAL_STACK_SIZE + 1024 * 4,
                NULL, tskIDLE_PRIORITY, &xTaskHandle);
  if (xReturned != pdPASS) {
    ESP_LOGE(TAG, "Error creating KWS event task");
    return -1;
  }

  s_event_cb = event_cb;
  return 0;
}

void kws_event_task_release() {
  if (xTaskHandle) {
    vTaskDelete(xTaskHandle);
    xTaskHandle = NULL;
  }
  s_event_cb = NULL;
}
