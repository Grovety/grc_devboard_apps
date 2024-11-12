#ifndef _I2S_RX_SLOT_H_
#define _I2S_RX_SLOT_H_

#include "stddef.h"
#include "stdint.h"

typedef enum {
  stLeft = 0,
  stRight,
  stBoth,
} eSlotType;

typedef struct rx_slot_conf_t {
  size_t sample_rate = 16000;
  eSlotType slot_type = stRight;
} rx_slot_conf_t;

/*!
 * \brief Initialize microphone rx slot.
 * \param conf Config.
 */
void i2s_rx_slot_init(const rx_slot_conf_t &conf);
/*!
 * \brief Enable microphone rx slot.
 */
void i2s_rx_slot_start();
/*!
 * \brief Stop microphone rx slot.
 */
void i2s_rx_slot_stop();
/*!
 * \brief Release microphone rx slot.
 */
void i2s_rx_slot_release();
/*!
 * \brief Read from rx slot.
 * \param buffer Pointer to buffer.
 * \param bytes Buffer size.
 * \param timeout_ms Wait timeout.
 */
int i2s_rx_slot_read(void *buffer, size_t bytes, size_t timeout_ms);

#endif // _I2S_RX_SLOT_H_
