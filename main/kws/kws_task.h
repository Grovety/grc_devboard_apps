#ifndef _KWS_TASK_H_
#define _KWS_TASK_H_

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"

#include "nn_model.h"

#define KWS_NUM_FBANK_BINS 40

#define KWS_NUM_MFCC    10
#define KWS_WIN_MS      40
#define KWS_STRIDE_MS   20
#define KWS_DURATION_MS 1000
#define KWS_FRAME_LEN   (CONFIG_KWS_SAMPLE_RATE / 1000 * KWS_WIN_MS)
#define KWS_FRAME_SHIFT (CONFIG_KWS_SAMPLE_RATE / 1000 * KWS_STRIDE_MS)
#define KWS_FRAME_NUM   ((KWS_DURATION_MS - KWS_WIN_MS) / KWS_STRIDE_MS + 1)

#define KWS_FEATURES_LEN KWS_FRAME_NUM *KWS_NUM_MFCC

/*! \brief Global KWS word request queue. */
extern QueueHandle_t xKWSRequestQueue;
/*! \brief Global KWS output category queue. */
extern QueueHandle_t xKWSResultQueue;
/*! \brief Global KWS event bits. */
extern EventGroupHandle_t xKWSEventGroup;

#define KWS_RUNNING_MSK BIT0
#define KWS_STOPPED_MSK BIT1

struct kws_task_conf_t {
  nn_model_handle_t model_handle;
  const nn_model_desc_t *model_desc;
};

/*!
 * \brief Initialize KWS task.
 * \param conf Configuration params.
 * \return Result.
 */
int kws_task_init(kws_task_conf_t conf);
/*!
 * \brief Release KWS task.
 * \return Result.
 */
void kws_task_release();
/*!
 * \brief Request to recognize words.
 * \param req_words Number of words.
 */
void kws_req_word(size_t req_words);
/*!
 * \brief Cancel request.
 */
void kws_req_cancel();

#endif // _KWS_TASK_H_
