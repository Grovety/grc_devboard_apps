#include "objects_table.h"
#include "App.hpp"
#include "ObjectsRecognition.h"
#include "bitmaps.h"
#include "eng_samples.h"
#include "models.h"

#include "esp_log.h"
#include "utils.h"

static constexpr char TAG[] = "objects_table";

static const nn_model_desc_t *nn_model_descs[] = {
  &numbers_model,
  &objects_model,
};

static const samples_table_t *ref_samples[] = {
  &ref_numbers_samples,
  &ref_objects_samples,
};

static object_info_t numbers_table[] = {
  {
    .label = "one",
    .recognizer = {},
    .ref_pronunciation = {},
    .data =
      {
        .type = object_info_t::NUMBER,
        .value =
          {
            .number = 1,
          },
      },
  },
  {
    .label = "two",
    .recognizer = {},
    .ref_pronunciation = {},
    .data =
      {
        .type = object_info_t::NUMBER,
        .value =
          {
            .number = 2,
          },
      },
  },
  {
    .label = "three",
    .recognizer = {},
    .ref_pronunciation = {},
    .data =
      {
        .type = object_info_t::NUMBER,
        .value =
          {
            .number = 3,
          },
      },
  },
};

static object_info_t objects_table[] = {
  {
    .label = "cat",
    .recognizer = {},
    .ref_pronunciation = {},
    .data =
      {
        .type = object_info_t::IMAGE_BITMAP,
        .value =
          {
            .image = cat_64_x_64_bits,
          },
      },
  },
  {
    .label = "dog",
    .recognizer = {},
    .ref_pronunciation = {},
    .data =
      {
        .type = object_info_t::IMAGE_BITMAP,
        .value =
          {
            .image = dog_64_x_64_bits,
          },
      },
  },
  {
    .label = "car",
    .recognizer = {},
    .ref_pronunciation = {},
    .data =
      {
        .type = object_info_t::IMAGE_BITMAP,
        .value =
          {
            .image = car_64_x_64_bits,
          },
      },
  },
  {
    .label = "house",
    .recognizer = {},
    .ref_pronunciation = {},
    .data =
      {
        .type = object_info_t::IMAGE_BITMAP,
        .value =
          {
            .image = house_64_x_64_bits,
          },
      },
  },
  {
    .label = "cat",
    .recognizer = {},
    .ref_pronunciation = {},
    .data =
      {
        .type = object_info_t::SAMPLE_IDX,
        .value =
          {
            .sample_idx = 1,
          },
      },
  },
  {
    .label = "dog",
    .recognizer = {},
    .ref_pronunciation = {},
    .data =
      {
        .type = object_info_t::SAMPLE_IDX,
        .value =
          {
            .sample_idx = 2,
          },
      },
  },
  {
    .label = "car",
    .recognizer = {},
    .ref_pronunciation = {},
    .data =
      {
        .type = object_info_t::SAMPLE_IDX,
        .value =
          {
            .sample_idx = 3,
          },
      },
  },
};

static const sub_scenario_desc_t s_numbers_scenario_descs[] = {
  {
    .object_info_table = numbers_table,
    .object_info_table_len = _countof(numbers_table),
    .transition_func =
      [](App *app, const object_info_t *object_info) {
        app->transition(new ObjectsRecognition(object_info));
      },
    .inference_threshold = NUMBERS_INFERENCE_THRESHOLD,
  },
};

static const sub_scenario_desc_t s_objects_scenario_descs[] = {
  {
    .object_info_table = objects_table,
    .object_info_table_len = _countof(objects_table),
    .transition_func =
      [](App *app, const object_info_t *object_info) {
        app->transition(new ObjectsRecognition(object_info));
      },
    .inference_threshold = OBJECTS_INFERENCE_THRESHOLD,
  },
};

void intro(App *app) {}

State *getMenu() {
  return new Menu(
    {
      {"All",
       [](App *app) {
         ESP_LOGI(TAG, "Use all objects");
         initSubScenario(app, {
                                {.descs = s_numbers_scenario_descs,
                                 .len = _countof(s_numbers_scenario_descs)},
                                {.descs = s_objects_scenario_descs,
                                 .len = _countof(s_objects_scenario_descs)},
                              });
         switchSubScenario(app);
       }},
      {"Numbers",
       [](App *app) {
         ESP_LOGI(TAG, "Use numbers subset");
         initSubScenario(app, {
                                {.descs = s_numbers_scenario_descs,
                                 .len = _countof(s_numbers_scenario_descs)},
                              });
         switchSubScenario(app);
       }},
      {"Objects",
       [](App *app) {
         ESP_LOGI(TAG, "Use objects subset");
         initSubScenario(app, {
                                {.descs = s_objects_scenario_descs,
                                 .len = _countof(s_objects_scenario_descs)},
                              });
         switchSubScenario(app);
       }},
    },
    {"Back", nullptr});
}

int init_objects_table() {
  // initialize fields in all objects
  struct {
    object_info_t *table;
    size_t len;
  } all_objects[] = {
    {
      .table = numbers_table,
      .len = _countof(numbers_table),
    },
    {
      .table = objects_table,
      .len = _countof(objects_table),
    },
  };

  for (size_t i = 0; i < _countof(all_objects); i++) {
    for (size_t j = 0; j < all_objects[i].len; j++) {
      auto &object_info = all_objects[i].table[j];
      const char *label = object_info.label;

      bool found = false;
      // find nn model and correct category idx
      for (size_t m = 0; m < _countof(nn_model_descs) && !found; m++) {
        const nn_model_desc_t *model_desc = nn_model_descs[m];

        for (size_t n = EXTRA_LABELS_OFFSET;
             n < model_desc->labels_num && !found; n++) {
          if (strcmp(label, model_desc->labels[n]) == 0) {
            found = true;
            object_info.recognizer.model_desc = model_desc;
            object_info.recognizer.label_idx = n;
          }
        }
      }
      if (!found) {
        ESP_LOGE(TAG, "Unable to find nn model for object: %s", label);
        return -1;
      }

      found = false;
      // find reference reference pronunciation
      for (size_t m = 0; m < _countof(ref_samples) && !found; m++) {
        auto &samples_table = ref_samples[m];

        for (size_t n = 0; n < samples_table->len && !found; n++) {
          if (strcmp(label, samples_table->samples[n].label) == 0) {
            found = true;
            object_info.ref_pronunciation.samples_table = samples_table;
            object_info.ref_pronunciation.sample_idx = n + 1;
          }
        }
      }
      if (!found) {
        ESP_LOGE(TAG, "Unable to find reference pronunciation for object: %s",
                 label);
        return -1;
      }
    }
  }

  return 0;
}

int release_objects_table() { return 0; }
