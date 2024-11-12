#ifndef _OBJECTS_RECOGNITION_H_
#define _OBJECTS_RECOGNITION_H_

#include <cstddef>
#include <vector>

#include "esp_random.h"

#include "App.hpp"
#include "VoiceMsgPlayer.hpp"
#include "nn_model.h"

#define EXTRA_LABELS_OFFSET 2

#define OBJECTS_ORDER_RANDOM
#define OBJ_IDX_RND_GEN_WINDOW_SZ 10

struct object_info_t {
  enum object_data_t {
    NOTHING,
    STRING,
    NUMBER,
    IMAGE_BITMAP,
    SAMPLE_IDX, // maybe use sample data instead of table idx
  };

  const char *label;

  struct {
    const nn_model_desc_t *model_desc = nullptr;
    int label_idx = -1;
  } recognizer;

  struct {
    const samples_table_t *samples_table = nullptr;
    int sample_idx = -1;
  } ref_pronunciation;

  struct {
    object_data_t type;
    union {
      const unsigned char *image;
      const char *str;
      size_t sample_idx;
      int number;
    } value;
  } const data;
};

struct sub_scenario_desc_t {
  object_info_t *object_info_table;
  const size_t object_info_table_len;
  void (*transition_func)(App *app, const object_info_t *object_info);
  const float inference_threshold;
};

struct sub_scenario_range_t {
  const sub_scenario_desc_t *descs;
  size_t len;
};

struct idx_generator {
  idx_generator() = default;
  idx_generator(size_t max_value) : counter_(0), max_value_(max_value) {
#ifdef OBJECTS_ORDER_RANDOM
    window_ = std::vector<size_t>(
      std::min(size_t(OBJ_IDX_RND_GEN_WINDOW_SZ), max_value - 1), size_t(-1));
#endif
  }

  size_t get_number() {
#ifdef OBJECTS_ORDER_RANDOM
    while (1) {
      const uint32_t rnd_num = esp_random();
      const size_t rnd_val = rnd_num % max_value_;

      bool has_duplicates = false;
      for (size_t i = 1; i <= window_.size(); i++) {
        const size_t idx = (counter_ + i) % window_.size();
        if (window_[idx] == rnd_val) {
          has_duplicates = true;
          break;
        }
      }
      if (!has_duplicates) {
        window_[counter_ % window_.size()] = rnd_val;
        counter_++;
        return rnd_val;
      }
    }
#endif
    return counter_++ % max_value_;
  }

private:
  size_t counter_;
  size_t max_value_;
#ifdef OBJECTS_ORDER_RANDOM
  std::vector<size_t> window_;
#endif
};

struct internal_state_t {
  nn_model_handle_t model_handle;
  idx_generator idx_gen;
  std::vector<size_t> objects_groups_lens;
  std::vector<sub_scenario_desc_t> sub_scenario_descs;
};

struct Menu : State {
  struct Item {
    const char *name;
    action_t action;
  };
  Menu(std::initializer_list<Item> item_list, Item back,
       VoiceMsgId v_msg_id = 0)
    : items_(item_list), current_item_(0), back_(back), v_msg_id_(v_msg_id) {}
  virtual ~Menu() = default;

  void enterAction(App *app) override;
  void exitAction(App *app) override;
  void handleEvent(App *app, Event_t ev) override;
  State *clone() { return new Menu(*this); }

protected:
  virtual void drawMenuItems(App *app) const;
  virtual void drawMenu(App *app) const;
  unsigned getCurrentItem() const { return current_item_ % items_.size(); }

  const std::vector<Item> items_;
  unsigned current_item_;
  unsigned max_lines_;
  Item back_;
  VoiceMsgId v_msg_id_;
  static size_t last_item_;
};

struct ObjectsRecognition : State {
  ObjectsRecognition(const object_info_t *const object_info)
    : attempt_num_(0), object_info_(object_info) {}
  void enterAction(App *app) override;
  void exitAction(App *app) override final;
  void handleEvent(App *app, Event_t ev) override final;
  State *clone() override { return new ObjectsRecognition(*this); }

protected:
  size_t attempt_num_;
  virtual void check_kws_result(App *app);
  virtual void reset_attempt(App *app);
  const object_info_t *const object_info_;
};

struct Arithmetic : ObjectsRecognition {
  using ObjectsRecognition::ObjectsRecognition;
  Arithmetic(const object_info_t *const object_info);
  void enterAction(App *app) override final;
  State *clone() override { return new Arithmetic(*this); }

protected:
  int terms_[2];
  int result_;
  char operator_;
};

State *getMenu();
void initObjectsRecognition(App *app);
void releaseObjectsRecognition(App *app);

void initSubScenario(App *app,
                     std::initializer_list<sub_scenario_range_t> ranges);
void releaseSubScenario();
void switchSubScenario(App *app);

#endif // _OBJECTS_RECOGNITION_H_
