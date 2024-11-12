#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"

struct Event_t {
  int id;
};

enum eEventId : int {
  NO_EVENT = 0,
  BUTTON_CLICK,
  BUTTON_HOLD,
  SW3_BUTTON_CLICK,
  SW4_BUTTON_CLICK,
  TIMEOUT,
  E_EVENT_ID_MAX,
};

/*!
 * \brief Global timer.
 */
extern TimerHandle_t xTimer;
/*!
 * \brief Global events queue.
 */
extern QueueHandle_t xEventQueue;
/*!
 * \brief Initialize Events Generator.
 * \return Result.
 */
int initEventsGenerator();
/*!
 * \brief Release Events Generator.
 */
void releaseEventsGenerator();
/*!
 * \brief Send event to events queue.
 * \param ev Event.
 */
void sendEvent(Event_t ev);
/*!
 * \brief Convert event to string.
 * \return String representation.
 */
const char *event_to_str(Event_t ev);
