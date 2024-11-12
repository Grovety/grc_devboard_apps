#include "App.hpp"
#include "Lcd.hpp"
#include "Status.hpp"
#include "git_version.h"
#include "models.h"
#include "sed_task.h"
#include "utils.h"

#define HEADER_STR " " TOSTRING(MAJOR_VERSION) "." TOSTRING(MINOR_VERSION)

static constexpr char TAG[] = "SED";

static nn_model_handle_t s_model_handle = NULL;

struct {
  const char *name;
  const nn_model_desc_t *model_desc;
  float inference_threshold;
  int mic_gain;
} static const scenario_desc {
#if CONFIG_SOUND_EVENTS_BABY_CRY
  .name = "baby_cry", .model_desc = &baby_cry_model,
  .inference_threshold = SED_INFERENCE_THRESHOLD, .mic_gain = 25,
#elif CONFIG_SOUND_EVENTS_GLASS_BREAKING
  .name = "glass_breaking", .model_desc = &glass_breaking_model,
  .inference_threshold = SED_INFERENCE_THRESHOLD, .mic_gain = 6,
#elif CONFIG_SOUND_EVENTS_BARK
  .name = "bark", .model_desc = &bark_model,
  .inference_threshold = SED_INFERENCE_THRESHOLD, .mic_gain = 20,
#elif CONFIG_SOUND_EVENTS_COUGHING
  .name = "coughing", .model_desc = &coughing_model,
  .inference_threshold = SED_INFERENCE_THRESHOLD, .mic_gain = 25,
#else
#error "set sound events type"
#endif
};

namespace SED {
struct Main : State {
  State *clone() override final { return new Main(*this); }
  void handleEvent(App *app, Event_t ev) override final {
    switch (ev.id) {
    default:
      update(app);
      break;
    }
  }
  void enterAction(App *app) override final {
    unsigned w, h;
    app->p_display->setFont(IDisplay::Font::CYR_6x12);
    app->p_display->get_font_sz(w, h);
    app->p_display->print_string(0, h, "%s %s", HEADER_STR, scenario_desc.name);
    app->p_display->setFont(IDisplay::Font::COURB24);
    app->p_display->get_font_sz(w, h);
    app->p_display->print_string(0, (DISPLAY_HEIGHT + h) / 2, "OFF");
    app->p_display->send();
  }
  void exitAction(App *app) override final {
    app->p_display->clear();
    app->p_display->send();
  }
  void update(App *app) {
    static char label[32];
    static int category;
    if (xQueuePeek(xSEDResultQueue, &category, 0) == pdPASS) {
      xEventGroupSetBits(xStatusEventGroup, STATUS_UNLOCKED_MSK);
      gpio_set_level(LOCK_PIN, 1);
      gpio_set_level(LOCK_PIN_INV, 0);
      nn_model_get_label(s_model_handle, category, label, sizeof(label));
      ESP_LOGI(TAG, "Detected: %s", label);
      unsigned w, h;
      app->p_display->setFont(IDisplay::Font::CYR_6x12);
      app->p_display->get_font_sz(w, h);
      app->p_display->print_string(0, h, "%s %s", HEADER_STR,
                                   scenario_desc.name);
      app->p_display->setFont(IDisplay::Font::COURB24);
      app->p_display->get_font_sz(w, h);
      app->p_display->print_string(0, (DISPLAY_HEIGHT + h) / 2, "ON");
      app->p_display->send();
      vTaskDelay(pdMS_TO_TICKS(1000));
      xQueueReceive(xSEDResultQueue, &category, 0);
      gpio_set_level(LOCK_PIN, 0);
      gpio_set_level(LOCK_PIN_INV, 1);
      xEventGroupClearBits(xStatusEventGroup, STATUS_UNLOCKED_MSK);
      app->p_display->setFont(IDisplay::Font::CYR_6x12);
      app->p_display->get_font_sz(w, h);
      app->p_display->print_string(0, h, "%s %s", HEADER_STR,
                                   scenario_desc.name);
      app->p_display->setFont(IDisplay::Font::COURB24);
      app->p_display->get_font_sz(w, h);
      app->p_display->print_string(0, (DISPLAY_HEIGHT + h) / 2, "OFF");
      app->p_display->send();
    }
  }
};
} // namespace SED

void releaseScenario(App *app) {
  ESP_LOGI(TAG, "Exiting SED scenairo");
  sed_task_release();
  if (s_model_handle) {
    nn_model_release(s_model_handle);
    s_model_handle = NULL;
  }
}

void initScenario(App *app) {
  ESP_LOGI(TAG, "Entering SED (%s) scenairo", scenario_desc.name);
  int errors = nn_model_init(&s_model_handle,
                             nn_model_config_t{
                               .model_desc = scenario_desc.model_desc,
                               .inference_threshold = SED_INFERENCE_THRESHOLD,
                             }) < 0;
  errors += sed_task_init(sed_task_conf_t{
              .model_handle = s_model_handle,
              .mic_gain = scenario_desc.mic_gain,
            }) < 0;
  if (errors) {
    ESP_LOGE(TAG, "SED init errors=%d", errors);
    app->transition(nullptr);
  } else {
    app->transition(new SED::Main);
  }
}
