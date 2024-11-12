#include "ObjectsRecognition.h"
#include "Event.hpp"
#include "Lcd.hpp"
#include "Status.hpp"
#include "VoiceMsgPlayer.hpp"

#include "bitmaps.h"
#include "kws_event_task.h"
#include "kws_task.h"
#include "models.h"
#include "objects_table.h"
#include "samples.h"
#include "utils.h"
#include "vad_task.h"

#include "bootloader_random.h"
#include "esp_random.h"

#include <climits>
#include <cstddef>
#include <string>
#include <sys/types.h>

static constexpr char TAG[] = "ObjectsRecognition";

#define OBJECT_SWITCH_TIMEOUT_MS (size_t(1000) * 7)
#define MAX_ATTEMPT_NUM          4

size_t Menu::last_item_ = 0;
static internal_state_t s_int_state;

#define KWS_WORD (eEventId::E_EVENT_ID_MAX + 1)

static void kws_event_cb(void *pv) {
  for (;;) {
    int category;
    if (xQueuePeek(xKWSResultQueue, &category, portMAX_DELAY) == pdPASS) {
      sendEvent({.id = KWS_WORD});
      xEventGroupWaitBits(xKWSEventGroup, KWS_STOPPED_MSK, pdFALSE, pdFALSE,
                          portMAX_DELAY);
    }
  }
}

static bool status_indicate_cb(ILed *p_led) {
  const auto xVADBits = xEventGroupGetBits(xVADEventGroup);

  if (xVADBits & VAD_RUNNING_MSK) {
    p_led->set(ILed::Blue);
    return true;
  } else {
    p_led->set(ILed::Black);
    return false;
  }
}

static size_t get_x_middle_offset(const char *str, size_t ch_width) {
  const size_t l = strlen(str);
  const size_t offset =
    std::min(size_t(float(l) / 2 * ch_width), size_t(DISPLAY_WIDTH / 2));
  return DISPLAY_WIDTH / 2 - offset;
};

void Menu::drawMenuItems(App *app) const {
  const unsigned num = getCurrentItem();
  unsigned i = num < max_lines_ ? 0 : (num - max_lines_ + 1);
  for (; i < items_.size(); ++i) {
    if (i == num) {
      app->p_display->print_string_ln("%d.%s<<", num + 1, items_[num].name);
    } else {
      app->p_display->print_string_ln("%d.%s", i + 1, items_[i].name);
    }
  }

  app->p_display->send();
}
void Menu::drawMenu(App *app) const { drawMenuItems(app); }
void Menu::enterAction(App *app) {
  app->p_display->setFont(IDisplay::Font::F8X13B);
  unsigned w, h;
  app->p_display->get_font_sz(w, h);
  max_lines_ = DISPLAY_HEIGHT / h;
  current_item_ = last_item_;
  VoiceMsgPlay(&voice_msg_samples, v_msg_id_);
  drawMenu(app);
}
void Menu::exitAction(App *app) {
  app->p_display->setFont(IDisplay::Font::COURB24);
  last_item_ = current_item_;
}
void Menu::handleEvent(App *app, Event_t ev) {
  switch (ev.id) {
  case eEventId::BUTTON_CLICK:
    current_item_++;
    drawMenu(app);
    break;
  case eEventId::BUTTON_HOLD:
    VoiceMsgStop();
    items_[getCurrentItem()].action(app);
    break;
  default:
    drawMenu(app);
    break;
  }
}

void ObjectsRecognition::handleEvent(App *app, Event_t ev) {
  switch (ev.id) {
  case eEventId::BUTTON_CLICK:
  case eEventId::BUTTON_HOLD:
    xTimerStop(xTimer, 0);
    kws_req_cancel();
    releaseSubScenario();
    app->transition(getMenu());
    break;
  case eEventId::SW3_BUTTON_CLICK:
  case eEventId::TIMEOUT:
    xTimerStop(xTimer, 0);
    kws_req_cancel();
    releaseSubScenario();
    switchSubScenario(app);
    break;
  case eEventId::SW4_BUTTON_CLICK:
    kws_req_cancel();
    reset_attempt(app);
    break;
  case KWS_WORD:
    xTimerStop(xTimer, 0);
    check_kws_result(app);
    break;
  default:
    break;
  }
}
void ObjectsRecognition::check_kws_result(App *app) {
  int category;
  if (xQueueReceive(xKWSResultQueue, &category, portMAX_DELAY) == pdPASS) {
    if (category == object_info_->recognizer.label_idx) {
      VoiceMsgPlay(&voice_msg_samples, 1);
      xEventGroupSetBits(xStatusEventGroup, STATUS_EVENT_GOOD_MSK);
      releaseSubScenario();
      VoiceMsgWaitStop(portMAX_DELAY);
      switchSubScenario(app);
    } else {
      xEventGroupSetBits(xStatusEventGroup, STATUS_EVENT_BAD_MSK);
      if (attempt_num_++ >= (MAX_ATTEMPT_NUM - 1)) {
        reset_attempt(app);
      } else {
        kws_req_word(1);
        xTimerChangePeriod(xTimer, pdMS_TO_TICKS(OBJECT_SWITCH_TIMEOUT_MS), 0);
      }
    }
  } else {
    ESP_LOGW(TAG, "Unable to receive KWS word");
  }
}
void ObjectsRecognition::reset_attempt(App *app) {
  attempt_num_ = 0;
  assert(object_info_);
  if (object_info_->ref_pronunciation.samples_table) {
    const auto &ref_pron = object_info_->ref_pronunciation;
    assert(ref_pron.samples_table);
    VoiceMsgPlay(ref_pron.samples_table, ref_pron.sample_idx);
  }
  ESP_LOGI(TAG, "label=%s", object_info_->label);

  unsigned w, h;
  app->p_display->setFont(IDisplay::Font::COURB14);
  app->p_display->get_font_sz(w, h);
  app->p_display->print_string(get_x_middle_offset(object_info_->label, w),
                               DISPLAY_HEIGHT / 2 + h / 2, "%s",
                               object_info_->label);
  app->p_display->send();

  app->p_display->setFont(IDisplay::Font::COURB24);

  VoiceMsgWaitStop(portMAX_DELAY);
  app->transition(clone());
}
void ObjectsRecognition::enterAction(App *app) {
  assert(object_info_);
  switch (object_info_->data.type) {
  case object_info_t::STRING: {
    ESP_LOGI(TAG, "Say letter %s", object_info_->data.value.str);

    unsigned w, h;
    app->p_display->setFont(IDisplay::Font::COURB14);
    app->p_display->get_font_sz(w, h);
    app->p_display->print_string(
      get_x_middle_offset(object_info_->data.value.str, w),
      DISPLAY_HEIGHT / 2 + h / 2, object_info_->data.value.str);
    app->p_display->send();
  } break;
  case object_info_t::NUMBER: {
    ESP_LOGI(TAG, "Say number %d", object_info_->data.value.number);

    const unsigned buf_size = 64;
    char buf[buf_size];
    snprintf(buf, buf_size, "%d", object_info_->data.value.number);
    unsigned w, h;
    app->p_display->get_font_sz(w, h);
    app->p_display->print_string(get_x_middle_offset(buf, w),
                                 DISPLAY_HEIGHT / 2 + h / 2, buf);
    app->p_display->send();
  } break;
  case object_info_t::IMAGE_BITMAP: {
    ESP_LOGI(TAG, "Object: %s", object_info_->label);
    app->p_display->clear();
    app->p_display->draw(32, 0, 64, 64, object_info_->data.value.image);
    app->p_display->send();
  } break;
  case object_info_t::SAMPLE_IDX: {
    app->p_display->clear();
    ESP_LOGI(TAG, "Sound object: %s", object_info_->label);
    VoiceMsgPlay(&sound_objects_samples, object_info_->data.value.sample_idx);
    VoiceMsgWaitStop(portMAX_DELAY);
  } break;
  default:
    break;
  }

  kws_req_word(1);
  xTimerChangePeriod(xTimer, pdMS_TO_TICKS(OBJECT_SWITCH_TIMEOUT_MS), 0);
}
void ObjectsRecognition::exitAction(App *app) {
  app->p_display->clear();
  app->p_display->send();
  VoiceMsgWaitStop(portMAX_DELAY);
}

Arithmetic::Arithmetic(const object_info_t *const object_info)
  : ObjectsRecognition(object_info) {
  assert(object_info_);
  assert(object_info_->data.type == object_info_t::NUMBER);

  static const char operators[]{
    '+',
    '-',
  };

  const int a = esp_random() % 20;
  const int result = object_info_->data.value.number;
  size_t op_idx = 0;

  int b = result - a;

  if (b <= 0) {
    op_idx = 1;
    b = a - result;
  }

  terms_[0] = a;
  terms_[1] = b;
  result_ = result;
  operator_ = operators[op_idx];
}
void Arithmetic::enterAction(App *app) {
  ESP_LOGI(TAG, "%d%c%d=%d", terms_[0], operator_, terms_[1], result_);

  const unsigned buf_size = 64;
  char buf[buf_size];
  snprintf(buf, buf_size, "%d%c%d", terms_[0], operator_, terms_[1]);
  unsigned w, h;
  app->p_display->get_font_sz(w, h);
  app->p_display->print_string(get_x_middle_offset(buf, w),
                               DISPLAY_HEIGHT / 2 + h / 2, buf);
  app->p_display->send();

  kws_req_word(1);
  xTimerChangePeriod(xTimer, pdMS_TO_TICKS(OBJECT_SWITCH_TIMEOUT_MS), 0);
}

void initSubScenario(App *app,
                     std::initializer_list<sub_scenario_range_t> ranges) {
  s_int_state.sub_scenario_descs.clear();
  s_int_state.objects_groups_lens = std::vector<size_t>(1, 0);

  size_t total_objects_num = 0;
  for (const auto &range : ranges) {
    for (size_t i = 0; i < range.len; i++) {
      total_objects_num += range.descs[i].object_info_table_len;
      s_int_state.objects_groups_lens.push_back(total_objects_num);
      s_int_state.sub_scenario_descs.push_back(range.descs[i]);
    }
  }
  ESP_LOGD(TAG, "total_objects_num=%u", total_objects_num);

  s_int_state.idx_gen = idx_generator(total_objects_num);
}

void releaseSubScenario() {
  kws_event_task_release();
  kws_task_release();
  if (s_int_state.model_handle) {
    nn_model_release(s_int_state.model_handle);
    s_int_state.model_handle = NULL;
  }
}

void switchSubScenario(App *app) {
  const size_t random_object_idx = s_int_state.idx_gen.get_number();
  size_t sub_scenario_idx = 1;
  while (random_object_idx >=
           s_int_state.objects_groups_lens[sub_scenario_idx] &&
         sub_scenario_idx < s_int_state.objects_groups_lens.size()) {
    sub_scenario_idx++;
  }
  sub_scenario_idx--;

  const size_t object_idx =
    random_object_idx - s_int_state.objects_groups_lens[sub_scenario_idx];
  ESP_LOGD(TAG, "random_object_idx=%u, object_idx=%u, sub_scenario_idx=%u",
           random_object_idx, object_idx, sub_scenario_idx);

  const nn_model_desc_t *model_desc =
    s_int_state.sub_scenario_descs[sub_scenario_idx]
      .object_info_table[object_idx]
      .recognizer.model_desc;
  int errors =
    nn_model_init(
      &s_int_state.model_handle,
      nn_model_config_t{
        .model_desc = model_desc,
        .inference_threshold =
          s_int_state.sub_scenario_descs[sub_scenario_idx].inference_threshold,
      }) < 0;
  errors += kws_task_init(kws_task_conf_t{
              .model_handle = s_int_state.model_handle,
              .model_desc = model_desc,
            }) < 0;
  errors += kws_event_task_init(&kws_event_cb);

  app->p_display->clear();
  app->p_display->send();

  if (errors) {
    ESP_LOGE(TAG, "KWS model init error");
    app->transition(nullptr);
  } else {
    s_int_state.sub_scenario_descs[sub_scenario_idx].transition_func(
      app, &s_int_state.sub_scenario_descs[sub_scenario_idx]
              .object_info_table[object_idx]);
  }
}

void initScenario(App *app) {
  ESP_LOGI(TAG, "Enter Objects Recongnition scenario");

  bootloader_random_enable();
  int errors = vad_task_init() < 0;
  errors = initWavPlayer() < 0;
  errors += init_objects_table() < 0;
  if (errors) {
    app->transition(nullptr);
  }

  addIndication(&status_indicate_cb);

  VoiceMsgPlay(&other_samples, 1);

  unsigned w, h;
  app->p_display->setFont(IDisplay::Font::CYR_10x20);
  app->p_display->get_font_sz(w, h);
  app->p_display->print_string(DISPLAY_WIDTH / 2 - 5 * w, DISPLAY_HEIGHT / 2,
                               "AI teacher");
  app->p_display->setFont(IDisplay::Font::CYR_6x12);
  app->p_display->get_font_sz(w, h);
  app->p_display->print_string(DISPLAY_WIDTH / 2 - 8 * w,
                               DISPLAY_HEIGHT / 2 + h, "english language");
  app->p_display->setFont(IDisplay::Font::COURR08);
  app->p_display->get_font_sz(w, h);
  app->p_display->print_string(DISPLAY_WIDTH - 5 * w, DISPLAY_HEIGHT, "© GRC");
  app->p_display->send();

  app->p_display->setFont(IDisplay::Font::COURB24);

  VoiceMsgWaitStop(portMAX_DELAY);
  app->transition(getMenu());
}

void releaseScenario(App *app) {
  ESP_LOGI(TAG, "Exit Objects Recongnition scenario");
  releaseSubScenario();
  s_int_state.sub_scenario_descs.clear();
  s_int_state.objects_groups_lens.clear();
  release_objects_table();
  releaseWavPlayer();
  vad_task_release();
  bootloader_random_disable();
}
