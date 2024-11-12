#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"

#include <array>
#include <memory>

#include "Event.hpp"
#include "IDisplay.hpp"
#include "State.hpp"
#include "Status.hpp"

#include "esp_log.h"

#include "driver/gpio.h"
#if CONFIG_TARGET_GRC_DEVBOARD
#define LOCK_PIN     GPIO_NUM_10
#define LOCK_PIN_INV GPIO_NUM_17
#elif CONFIG_TARGET_AI_MODULE
#define LOCK_PIN     GPIO_NUM_39
#define LOCK_PIN_INV GPIO_NUM_41
#else
#error "unknown target"
#endif

typedef void (*action_t)(App *app);

void initScenario(App *app);
void releaseScenario(App *app);

/*!
 * \brief Simple fsm.
 */
class App {
public:
  /*!
   * \brief Constructor.
   */
  App();
  /*!
   * \brief Destructor.
   */
  ~App();
  /*!
   * \brief Start processing events.
   */
  void run();
  /*!
   * \brief State transition.
   * \param target_state Pointer to next state.
   */
  void transition(State *target_state);

  /*! \brief Pointer to led. */
  std::unique_ptr<ILed> p_led;
  /*! \brief Pointer to display. */
  std::unique_ptr<IDisplay> p_display;

private:
  /*!
   * \brief State transition impl.
   * \param target_state Pointer to next state.
   */
  void do_transition(State *target_state);

private:
  /*! \brief Queue of transitions. */
  QueueHandle_t transition_queue_;
  /*! \brief Current state of the application. */
  State *current_state_;
};
