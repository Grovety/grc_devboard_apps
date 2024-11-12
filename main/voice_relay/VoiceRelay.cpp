#include "App.hpp"
#include "Lcd.hpp"
#include "git_version.h"
#include "kws_event_task.h"
#include "kws_task.h"
#include "model.h"
#include "utils.h"
#include "vad_task.h"

#if SUSPEND_TIMEOUT_S > 0
#define SUSPEND_AFTER_TICKS pdMS_TO_TICKS(1000 * SUSPEND_TIMEOUT_S)
#else
#define SUSPEND_AFTER_TICKS pdMS_TO_TICKS(1000 * 10)
#endif

#define TITLE      "VoiceRelay"
#define HEADER_STR TITLE " " TOSTRING(MAJOR_VERSION) "." TOSTRING(MINOR_VERSION)

static constexpr char TAG[] = TITLE;

static nn_model_handle_t s_model_handle = NULL;

enum eVoiceRelayEventId : unsigned {
  ROBOT = eEventId::E_EVENT_ID_MAX + 1,
  STOP
};

static void kws_event_cb(void *pv) {
  char result[32];
  for (;;) {
    int category;
    if (xQueueReceive(xKWSResultQueue, &category, portMAX_DELAY) == pdPASS) {
      nn_model_get_label(s_model_handle, category, result, sizeof(result));
      if (strcmp(result, "robot") == 0) {
        sendEvent({.id = eVoiceRelayEventId::ROBOT});
      } else if (strcmp(result, "stop") == 0) {
        sendEvent({.id = eVoiceRelayEventId::STOP});
      }
    }
  }
}

namespace VoiceRelay {
struct Suspended : State {
  Suspended(State *back_state) : back_state_(back_state) {}
  State *clone() { return new Suspended(*this); }
  ~Suspended() {
    if (back_state_) {
      delete back_state_;
    }
  }

  void enterAction(App *app) {
    ESP_LOGI(TAG, "Entering suspended state");
    xEventGroupSetBits(xStatusEventGroup, STATUS_SYSTEM_SUSPENDED_MSK);
    kws_req_word(1);

    unsigned w, h;
    app->p_display->setFont(IDisplay::Font::CYR_6x12);
    app->p_display->get_font_sz(w, h);
    app->p_display->print_string(0, h, HEADER_STR);

    app->p_display->setFont(IDisplay::Font::COURB24);
    app->p_display->get_font_sz(w, h);
    app->p_display->print_string(0, (DISPLAY_HEIGHT + h) / 2, "OFF");

    app->p_display->setFont(IDisplay::Font::CYR_6x12);
    app->p_display->get_font_sz(w, h);
    app->p_display->print_string(0, DISPLAY_HEIGHT, "robot --> ON");
    app->p_display->send();
  }
  void exitAction(App *app) {
    xEventGroupClearBits(xStatusEventGroup, STATUS_SYSTEM_SUSPENDED_MSK);
    ESP_LOGI(TAG, "Exiting suspended state");
    app->p_display->clear();
    app->p_display->send();
  }
  void handleEvent(App *app, Event_t ev) {
    switch (ev.id) {
    case eEventId::BUTTON_CLICK:
      app->transition(back_state_);
      back_state_ = nullptr;
      kws_req_cancel();
      break;
    case eVoiceRelayEventId::ROBOT:
      app->transition(back_state_);
      back_state_ = nullptr;
      break;
    default:
      if (uxQueueMessagesWaiting(xKWSRequestQueue) == 0) {
        kws_req_word(1);
      }
      break;
    }
  }

protected:
  State *back_state_;
};

struct Main : State {
  Main() : timer_val_s_(0) {}

  State *clone() { return new Main(*this); }
  void enterAction(App *app) {
    ESP_LOGI(TAG, "Entering main state");
    kws_req_word(1);
    gpio_set_level(LOCK_PIN, 1);
    gpio_set_level(LOCK_PIN_INV, 0);
    xEventGroupSetBits(xStatusEventGroup, STATUS_UNLOCKED_MSK);
    xTimerChangePeriod(xTimer, SUSPEND_AFTER_TICKS, 0);
    timer_val_s_ = float(SUSPEND_AFTER_TICKS * portTICK_PERIOD_MS) / 1000;
    ESP_LOGD(TAG, "timer_val_s_=%d", timer_val_s_);
  }
  void exitAction(App *app) {
    ESP_LOGI(TAG, "Exiting main state");
    xEventGroupClearBits(xStatusEventGroup, STATUS_UNLOCKED_MSK);
    gpio_set_level(LOCK_PIN, 0);
    gpio_set_level(LOCK_PIN_INV, 1);
    app->p_display->clear();
    app->p_display->send();
  }
  void handleEvent(App *app, Event_t ev) {
    switch (ev.id) {
    case eVoiceRelayEventId::STOP:
      app->transition(new Suspended(clone()));
      break;
    case eEventId::BUTTON_CLICK:
    case eEventId::TIMEOUT:
      kws_req_cancel();
      app->transition(new Suspended(clone()));
      break;
    default:
      if (uxQueueMessagesWaiting(xKWSRequestQueue) == 0) {
        kws_req_word(1);
      } else {
        update(app);
      }
      break;
    }
  }
  void update(App *app) {
    const TickType_t xRemainingTime =
      xTimerGetExpiryTime(xTimer) - xTaskGetTickCount();
    const size_t rem_time_s = float(xRemainingTime * portTICK_PERIOD_MS) / 1000;
    if (timer_val_s_ != rem_time_s) {
      timer_val_s_ = rem_time_s;
      ESP_LOGD(TAG, "timer_val_s_=%d", timer_val_s_);

      unsigned w, h;
      app->p_display->setFont(IDisplay::Font::CYR_6x12);
      app->p_display->get_font_sz(w, h);
      app->p_display->print_string(0, h, HEADER_STR);

      app->p_display->setFont(IDisplay::Font::COURB24);
      app->p_display->get_font_sz(w, h);
      app->p_display->print_string(0, (DISPLAY_HEIGHT + h) / 2, "ON: %d",
                                   timer_val_s_ + 1);

      app->p_display->setFont(IDisplay::Font::CYR_6x12);
      app->p_display->get_font_sz(w, h);
      app->p_display->print_string(0, DISPLAY_HEIGHT, "stop --> OFF");
      app->p_display->send();
    }
  }

private:
  size_t timer_val_s_;
};
} // namespace VoiceRelay

void releaseScenario(App *app) {
  ESP_LOGI(TAG, "Exiting VoiceRelay scenairo");
  kws_event_task_release();
  kws_task_release();
  if (s_model_handle) {
    nn_model_release(s_model_handle);
    s_model_handle = NULL;
  }
  vad_task_release();
}

void initScenario(App *app) {
  int errors = vad_task_init() < 0;
  errors += nn_model_init(&s_model_handle, nn_model_config_t{
                                             .model_desc = &voice_relay_model,
                                             .inference_threshold =
                                               VOICE_RELAY_INFERENCE_THRESHOLD,
                                           }) < 0;
  errors += kws_task_init(kws_task_conf_t{
              .model_handle = s_model_handle,
              .model_desc = &voice_relay_model,
            }) < 0;
  errors += kws_event_task_init(&kws_event_cb);

  if (errors) {
    ESP_LOGE(TAG, "Unable to init KWS");
    app->transition(nullptr);
  } else {
    ESP_LOGI(TAG, "Entering VoiceRelay scenairo");
    app->transition(new VoiceRelay::Suspended(new VoiceRelay::Main));
  }
}
