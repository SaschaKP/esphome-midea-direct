#pragma once
#include <cstdint>
#include <functional>
#include <list>
#include "esphome/core/hal.h"

namespace esphome {
namespace midea {

class Timer;
using TimerTick = uint32_t;
using TimerCallback = std::function<void(Timer *)>;
using Timers = std::list<Timer *>;

class TimerManager {
  public:
  static TimerTick ms() { return esphome::millis(); }
  void registerTimer(Timer &timer) { timers_.push_back(&timer); }
  void task();

  private:
  Timers timers_;
};

class Timer {
  public:
  Timer();
  bool isExpired() const { return TimerManager::ms() - this->last_ >= this->alarm_; }
  bool isEnabled() const { return this->alarm_; }
  void start(TimerTick ms) {
    this->alarm_ = ms;
    this->reset();
  }
  void stop() { this->alarm_ = 0; }
  void reset() { this->last_ = TimerManager::ms(); }
  void setCallback(TimerCallback cb) { this->callback_ = cb; }
  void call() { this->callback_(this); }
  private:
  // Callback function or lambda
  TimerCallback callback_;
  // Period of operation
  TimerTick alarm_;
  // Last time of operation
  TimerTick last_;
};

}  // namespace midea
}  // namespace esphome
