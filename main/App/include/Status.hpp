#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

#include "ILed.hpp"

#define GET_BIT_POS(mask) 31 - __builtin_clz(mask)
#define ALL_BITS(pos)     BIT##pos - 1

#define STATUS_UNLOCKED_MSK         BIT0
#define STATUS_EVENT_GOOD_MSK       BIT1
#define STATUS_EVENT_BAD_MSK        BIT2
#define STATUS_STATE_BAD_MSK        BIT3
#define STATUS_SYSTEM_SUSPENDED_MSK BIT4
#define STATUS_ALL_BITS_MSK         ALL_BITS(6)
#define STATUS_EVENT_BITS_MSK       (STATUS_EVENT_GOOD_MSK | STATUS_EVENT_BAD_MSK)

typedef bool (*indicate_cb_t)(ILed *);

/*!
 * \brief Global status bits.
 */
extern EventGroupHandle_t xStatusEventGroup;
/*!
 * \brief Initialize Status Monitor.
 * \param p_led Pointer to led device.
 * \return Result.
 */
int initStatusMonitor(ILed *p_led);
/*!
 * \brief Add indication function.
 * \param indicate_cb Indication function.
 */
void addIndication(indicate_cb_t indicate_cb);
/*!
 * \brief Release Status Monitor.
 */
void releaseStatusMonitor();
