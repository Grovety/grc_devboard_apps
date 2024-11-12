#ifndef _VAD_TASK_H_
#define _VAD_TASK_H_

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"

#include "def.h"

#define VAD_RUNNING_MSK BIT0
#define VAD_STOPPED_MSK BIT1
#define VAD_STOP_MSK    BIT2

#define DET_FRAME_LEN MIC_FRAME_LEN
#define DET_FRAME_SZ  (DET_FRAME_LEN * MIC_ELEM_BYTES)

#define DET_IS_VOICED_THRESHOLD       0.90
#define DET_VOICED_FRAMES_WINDOW      26
#define DET_VOICED_FRAMES_THRESHOLD   12
#define DET_UNVOICED_FRAMES_THRESHOLD 2

#define DET_MAX_WORDS 1
#define DET_WORD_BUF_FRAME_NUM                                                 \
  (DET_MAX_WORDS * CONFIG_KWS_SAMPLE_RATE / DET_FRAME_LEN)
#define DET_WORD_BUF_SZ (DET_WORD_BUF_FRAME_NUM * DET_FRAME_SZ)

struct WordDesc_t {
  size_t frame_num;
  size_t max_abs;
};

/*! \brief Global word frames buffer. */
extern StreamBufferHandle_t xWordFramesBuffer;
/*! \brief Global input word queue. */
extern QueueHandle_t xWordQueue;
/*! \brief Global VAD event bits. */
extern EventGroupHandle_t xVADEventGroup;

/*!
 * \brief Initialize VAD task.
 * \return Result.
 */
int vad_task_init();
/*!
 * \brief Release VAD task.
 */
void vad_task_release();
/*!
 * \brief Start VAD task.
 */
void vad_task_start();
/*!
 * \brief Stop VAD task.
 */
void vad_task_stop();

#endif // _VAD_TASK_H_
