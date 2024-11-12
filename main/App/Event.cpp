#include "Event.hpp"
#include "Button.hpp"

enum eButtonState { DOWN, UP };

#define BUTTON_HOLD_DURATION_MS 2000

static constexpr char TAG[] = "Event";

static TaskHandle_t xBTaskHandle = NULL;
static TaskHandle_t xSW3TaskHandle = NULL;
static TaskHandle_t xSW4TaskHandle = NULL;
TimerHandle_t xTimer = NULL;
QueueHandle_t xEventQueue = NULL;

size_t getTimeMS() { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

void sendEvent(Event_t ev) {
  if (xQueueSend(xEventQueue, &ev, 0) == pdPASS) {
    ESP_LOGD(TAG, "send ev=%s", event_to_str(ev));
  } else {
    ESP_LOGD(TAG, "skip ev=%s", event_to_str(ev));
  }
}

static void button_event_task(void *pvParameters) {
  Button int_button(INT_BUTTON_PIN);
  eButtonState bstate = eButtonState::UP;
  unsigned start_time = 0;
  bool pending_hold = false;

  auto get_bstate = [&int_button]() {
    const bool s_b1 = int_button.isPressed();
    ESP_LOGV(TAG, "buttons state: %d", s_b1);
    return !s_b1;
  };

  for (;;) {
    if (bstate == eButtonState::UP) {
      if (get_bstate()) {
        bstate = eButtonState::DOWN;
        start_time = getTimeMS();
      }
    } else if (bstate == eButtonState::DOWN) {
      if (!pending_hold) {
        const auto cur_time = getTimeMS();
        if (cur_time > start_time + BUTTON_HOLD_DURATION_MS) {
          pending_hold = true;
          sendEvent({.id = eEventId::BUTTON_HOLD});
        } else if (!get_bstate()) {
          bstate = eButtonState::UP;
          sendEvent({.id = eEventId::BUTTON_CLICK});
        }
      } else if (!get_bstate()) {
        pending_hold = false;
        bstate = eButtonState::UP;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static void sw3_buttons_event_task(void *pvParameters) {
  Button sw3(SW3_BUTTON_PIN);
  eButtonState bstate = eButtonState::UP;
  unsigned start_time = 0;
  bool pending_hold = false;

  auto get_bstate = [&sw3]() {
    const bool sw3_state = sw3.isPressed();
    ESP_LOGV(TAG, "sw state: %d", sw3_state);
    return !sw3_state;
  };

  for (;;) {
    if (bstate == eButtonState::UP) {
      if (get_bstate()) {
        bstate = eButtonState::DOWN;
        start_time = getTimeMS();
      }
    } else if (bstate == eButtonState::DOWN) {
      if (!pending_hold) {
        if (getTimeMS() > start_time + BUTTON_HOLD_DURATION_MS) {
          pending_hold = true;
          sendEvent({.id = eEventId::SW3_BUTTON_CLICK});
        } else if (!get_bstate()) {
          bstate = eButtonState::UP;
          sendEvent({.id = eEventId::SW3_BUTTON_CLICK});
        }
      } else if (!get_bstate()) {
        pending_hold = false;
        bstate = eButtonState::UP;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static void sw4_buttons_event_task(void *pvParameters) {
  Button sw4(SW4_BUTTON_PIN);
  eButtonState bstate = eButtonState::UP;
  unsigned start_time = 0;
  bool pending_hold = false;

  auto get_bstate = [&sw4]() {
    const bool sw4_state = sw4.isPressed();
    ESP_LOGV(TAG, "sw state: %d", sw4_state);
    return !sw4_state;
  };

  for (;;) {
    if (bstate == eButtonState::UP) {
      if (get_bstate()) {
        bstate = eButtonState::DOWN;
        start_time = getTimeMS();
      }
    } else if (bstate == eButtonState::DOWN) {
      if (!pending_hold) {
        if (getTimeMS() > start_time + BUTTON_HOLD_DURATION_MS) {
          pending_hold = true;
          sendEvent({.id = eEventId::SW4_BUTTON_CLICK});
        } else if (!get_bstate()) {
          bstate = eButtonState::UP;
          sendEvent({.id = eEventId::SW4_BUTTON_CLICK});
        }
      } else if (!get_bstate()) {
        pending_hold = false;
        bstate = eButtonState::UP;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static void vTimerCallback(TimerHandle_t xTimer) {
  ESP_LOGD(TAG, "send timeout event");
  sendEvent(Event_t{.id = eEventId::TIMEOUT});
}

int initEventsGenerator() {
  xEventQueue = xQueueCreate(5, sizeof(Event_t));
  if (xEventQueue == NULL) {
    ESP_LOGE(TAG, "Error creating event queue");
    return -1;
  }
  xTimer =
    xTimerCreate("Timer", pdMS_TO_TICKS(1000), pdFALSE, NULL, vTimerCallback);
  if (xTimer == NULL) {
    ESP_LOGE(TAG, "Error creating xTimer");
    return -1;
  } else {
    xTimerStop(xTimer, 0);
  }

  auto xReturned = xTaskCreate(button_event_task, "button_event_task",
                               configMINIMAL_STACK_SIZE + 1024, NULL,
                               tskIDLE_PRIORITY, &xBTaskHandle);
  if (xReturned != pdPASS) {
    ESP_LOGE(TAG, "Error creating button event task");
    return -1;
  }
  xReturned = xTaskCreate(sw3_buttons_event_task, "sw3_buttons_event_task",
                          configMINIMAL_STACK_SIZE + 1024, NULL,
                          tskIDLE_PRIORITY, &xSW3TaskHandle);
  if (xReturned != pdPASS) {
    ESP_LOGE(TAG, "Error creating sw buttons event task");
    return -1;
  }
  xReturned = xTaskCreate(sw4_buttons_event_task, "sw4_buttons_event_task",
                          configMINIMAL_STACK_SIZE + 1024, NULL,
                          tskIDLE_PRIORITY, &xSW4TaskHandle);
  if (xReturned != pdPASS) {
    ESP_LOGE(TAG, "Error creating sw buttons event task");
    return -1;
  }
  return 0;
}

void releaseEventsGenerator() {
  if (xBTaskHandle) {
    vTaskDelete(xBTaskHandle);
    xBTaskHandle = NULL;
  }
  if (xSW3TaskHandle) {
    vTaskDelete(xSW3TaskHandle);
    xSW3TaskHandle = NULL;
  }
  if (xSW4TaskHandle) {
    vTaskDelete(xSW4TaskHandle);
    xSW4TaskHandle = NULL;
  }
  if (xEventQueue) {
    vQueueDelete(xEventQueue);
    xEventQueue = NULL;
  }
  if (xTimer) {
    xTimerStop(xTimer, 0);
    xTimerDelete(xTimer, 0);
    xTimer = NULL;
  }
}

const char *event_to_str(Event_t ev) {
  switch (ev.id) {
  case eEventId::NO_EVENT:
    return "NO_EVENT";
    break;
  case eEventId::BUTTON_CLICK:
    return "BUTTON_CLICK";
    break;
  case eEventId::BUTTON_HOLD:
    return "BUTTON_HOLD";
    break;
  case eEventId::SW3_BUTTON_CLICK:
    return "SW3_BUTTON_CLICK";
    break;
  case eEventId::SW4_BUTTON_CLICK:
    return "SW4_BUTTON_CLICK";
    break;
  case eEventId::TIMEOUT:
    return "TIMEOUT";
    break;
  default:
    return "";
  }
}
