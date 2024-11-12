#include "App.hpp"
#include "Button.hpp"
#include "DisplaySTDOUT.hpp"
#include "Lcd.hpp"
#include "Led.hpp"
#include "State.hpp"
#include "Status.hpp"

#include "mic_reader.h"

static constexpr char TAG[] = "App";

App::App() : current_state_(nullptr) {
  int errors = 0;
  transition_queue_ = xQueueCreate(1, sizeof(State *));
  if (transition_queue_ == NULL) {
    ESP_LOGE(TAG, "Error creating transition queue");
    errors++;
  }
  p_led = std::make_unique<Led>();

#if CONFIG_TARGET_GRC_DEVBOARD
  p_display = std::make_unique<Lcd>(Lcd::Rotation::PORTRAIT);
#else
  p_display = std::make_unique<DisplaySTDOUT>();
#endif

  p_display->clear();
  p_display->send();

  const auto result = mic_reader_init();
  switch (result) {
  case MIC_SILENT:
    ESP_LOGE(TAG, "Mic is silent");
    errors++;
    break;
  case MIC_INIT_ERROR:
    ESP_LOGE(TAG, "Mic init error");
    errors++;
    break;
  case MIC_NOISY:
    ESP_LOGE(TAG, "Mic is noisy");
    errors++;
    break;
  default:
    break;
  }

  errors += initEventsGenerator() < 0;
  errors += initStatusMonitor(p_led.get()) < 0;

  if (errors) {
    ESP_LOGE(TAG, "Init errors=%d", errors);
    xEventGroupSetBits(xStatusEventGroup, STATUS_STATE_BAD_MSK);
    vTaskDelay(portMAX_DELAY);
  }

  initScenario(this);
}

App::~App() {
  initScenario(this);

  p_display->clear();
  p_display->send();

  p_led->set(0);

  State *state;
  while (xQueueReceive(transition_queue_, &state, 0) == pdPASS) {
    delete state;
  }
  if (current_state_) {
    delete current_state_;
  }
  vQueueDelete(transition_queue_);
  releaseEventsGenerator();
  releaseStatusMonitor();
  mic_reader_release();
}

void App::transition(State *target_state) {
  xQueueSend(transition_queue_, &target_state, portMAX_DELAY);
}

void App::do_transition(State *target_state) {
  if (current_state_ != nullptr) {
    current_state_->exitAction(this);
    delete current_state_;
    current_state_ = nullptr;
  }
  if (target_state != nullptr) {
    target_state->enterAction(this);
  }
  current_state_ = target_state;
  xQueueReset(xEventQueue);
}

void App::run() {
  const TickType_t xTicksToWait = pdMS_TO_TICKS(100);

  for (;;) {
    State *target_state = nullptr;
    if (xQueueReceive(transition_queue_, &target_state, 0) == pdPASS) {
      do_transition(target_state);
    }
    if (current_state_ == nullptr) {
      return;
    }
    Event_t ev = {};
    if (xQueueReceive(xEventQueue, &ev, xTicksToWait) == pdPASS) {
      current_state_->handleEvent(this, ev);
    } else {
      current_state_->handleEvent(this, {.id = eEventId::NO_EVENT});
    }
  }
}
