#pragma once
#include "Event.hpp"

class App;

/*!
 * \brief App state interface.
 */
struct State {
  virtual ~State() = default;
  virtual void enterAction(App *) {}
  virtual void exitAction(App *) {}
  virtual void handleEvent(App *, Event_t) {}
  virtual State *clone() = 0;
};
