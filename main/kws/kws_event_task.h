#pragma once

typedef void (*kws_event_cb_t)(void *);

/*!
 * \brief Initialize Key Word Selection system.
 * \param event_cb Event generation function.
 * \return Result.
 */
int kws_event_task_init(kws_event_cb_t event_cb);
/*!
 * \brief Release Key Word Selection system.
 */
void kws_event_task_release();
