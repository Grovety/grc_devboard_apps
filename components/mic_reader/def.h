#ifndef _DEF_H_
#define _DEF_H_

#include "stddef.h"
#include "stdint.h"

#include "driver/gpio.h"

#if CONFIG_TARGET_GRC_DEVBOARD
#define MIC_WS_PIN   GPIO_NUM_38
#define MIC_DATA_PIN GPIO_NUM_45
#elif CONFIG_TARGET_AI_MODULE
#define MIC_WS_PIN   GPIO_NUM_48
#define MIC_DATA_PIN GPIO_NUM_47
#else
#error "unknown target"
#endif

#if (CONFIG_MIC_CHANNEL_RIGHT | CONFIG_MIC_CHANNEL_LEFT)
#define MIC_CHANNEL_NUM 1
#elif CONFIG_MIC_CHANNEL_BOTH
#define MIC_CHANNEL_NUM 2
#else
#define MIC_CHANNEL_NUM 1
#endif

typedef int16_t audio_t;
#define MIC_FRAME_LEN_MS 10
#define MIC_ELEM_BYTES   sizeof(audio_t)
#define MIC_FRAME_LEN    (CONFIG_MIC_SAMPLE_RATE / 1000 * MIC_FRAME_LEN_MS)
#define MIC_FRAME_SZ     (MIC_FRAME_LEN * MIC_ELEM_BYTES * MIC_CHANNEL_NUM)

#define I2S_RX_PORT_NUMBER I2S_NUM_0
#define I2S_RX_BIT_WIDTH   I2S_DATA_BIT_WIDTH_16BIT
#define I2S_RX_SLOT_MODE   I2S_SLOT_MODE_MONO

#define I2S_RX_DMA_BUF_LEN (MIC_FRAME_LEN * MIC_CHANNEL_NUM)
#define I2S_RX_DMA_BUF_SZ  MIC_FRAME_SZ
#define I2S_RX_DMA_BUF_NUM 2

#endif // _DEF_H_
