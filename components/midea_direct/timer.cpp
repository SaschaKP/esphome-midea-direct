#include "timer.h"

namespace esphome {
namespace midea {

// Dummy function for incorrect using case.
static void dummy(Timer *timer) { timer->stop(); }
Timer::Timer() : callback_(dummy), alarm_(0) {}

/// Timers task. Must be periodically called in loop function.
void TimerManager::task() {
  for (auto timer : timers_)
    if (timer->isEnabled() && timer->isExpired())
      timer->call();
}

}  // namespace midea
}  // namespace esphome
