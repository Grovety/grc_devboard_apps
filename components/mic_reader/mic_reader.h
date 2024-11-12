#ifndef _MIC_READER_H_
#define _MIC_READER_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "def.h"

typedef enum MicResult_e {
  MIC_OK,
  MIC_SILENT,
  MIC_NOISY,
  MIC_INIT_ERROR,
} MicResult_t;

/*! \brief Global microphone semaphore. */
extern SemaphoreHandle_t xMicSema;

/*!
 * \brief Initialize microphone data reader and processor.
 * \return Result.
 */
MicResult_t mic_reader_init();
/*!
 * \brief Release microphone data reader and processor.
 */
void mic_reader_release();
/*!
 * \brief Read MIC_FRAME_LEN samples from mic.
 * \param buffer Preallocated buffer.
 * \param timeout_ms Wait timeout.
 * \return Result.
 */
int mic_reader_read_frame(audio_t *buffer, size_t timeout_ms);

#endif // _MIC_READER_H_
