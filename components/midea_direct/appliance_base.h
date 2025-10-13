#pragma once
#include <deque>
#include <optional>
#include "esphome/components/uart/uart.h"
#include "frame.h"
#include "frame_data.h"
#include "timer.h"

namespace esphome {
namespace midea {

// Use C++17 std::optional instead of custom Optional class
template<typename T>
using Optional = std::optional<T>;

enum ApplianceType : uint8_t {
  DEHUMIDIFIER = 0xA1,
  AIR_CONDITIONER = 0xAC,
  AIR2WATER = 0xC3,
  FAN = 0xFA,
  CLEANER = 0xFC,
  HUMIDIFIER = 0xFD,
  BROADCAST = 0xFF
};

enum AutoconfStatus : uint8_t {
  AUTOCONF_DISABLED,
  AUTOCONF_PROGRESS,
  AUTOCONF_OK,
  AUTOCONF_ERROR,
};

enum ResponseStatus : uint8_t {
  RESPONSE_OK,
  RESPONSE_PARTIAL,
  RESPONSE_WRONG,
};

enum RequestPriority : uint8_t {
  PRIORITY_BACKGROUND,    // Status queries, power usage, etc.
  PRIORITY_USER_COMMAND,  // User-initiated commands (highest priority)
  PRIORITY_USER_SEQUENCE, // Sequenced user commands with delays
};

enum FrameType : uint8_t {
  DEVICE_CONTROL = 0x02,
  DEVICE_QUERY = 0x03,
  GET_ELECTRONIC_ID = 0x07,
  NETWORK_NOTIFY = 0x0D,
  QUERY_NETWORK = 0x63,
};

using Handler = std::function<void()>;
using ResponseHandler = std::function<ResponseStatus(FrameData)>;
using OnStateCallback = std::function<void()>;

class ApplianceBase {
 public:
  ApplianceBase(ApplianceType type) : appType_(type) {}
  /// Setup
  void setup();
  /// Loop
  void loop();

  /* ############################## */
  /* ### COMMUNICATION SETTINGS ### */
  /* ############################## */

  /// Set UART device
  void setUARTDevice(uart::UARTDevice *uart_device) { this->uart_device_ = uart_device; }
  /// Set minimal period between requests
  void setPeriod(uint32_t period) { this->period_ = period; }
  uint32_t getPeriod() const { return this->period_; }
  /// Set waiting response timeout
  void setTimeout(uint32_t timeout) { this->timeout_ = timeout; }
  uint32_t getTimeout() const { return this->timeout_; }
  /// Set number of request attempts
  void setNumAttempts(uint8_t numAttempts) { this->numAttempts_ = numAttempts; }
  uint8_t getNumAttempts() const { return this->numAttempts_; }
  /// Set beeper feedback
  void setBeeper(bool value);
  /// Add listener for appliance state
  void addOnStateCallback(OnStateCallback cb) { this->state_callbacks_.push_back(cb); }
  void sendUpdate() {
    // Optimize for common case of no callbacks
    if (!this->state_callbacks_.empty()) {
      for (auto &cb : this->state_callbacks_)
        cb();
    }
  }
  AutoconfStatus getAutoconfStatus() const { return this->autoconf_status_; }
  void setAutoconf(bool state) { this->autoconf_status_ = state ? AUTOCONF_PROGRESS : AUTOCONF_DISABLED; }

 protected:
  std::vector<OnStateCallback> state_callbacks_;
  // Timer manager
  TimerManager timer_manager_;
  AutoconfStatus autoconf_status_;
  // Beeper feedback flag
  bool beeper_;
  // User command tracking
  bool has_pending_user_command_;
  uint32_t last_user_command_time_;
  // Sequenced command tracking
  bool is_in_sequence_mode_;
  uint32_t sequence_start_time_;
  uint32_t last_sequence_command_time_;

  struct Request {
    FrameData request;
    ResponseHandler onData;
    Handler onSuccess;
    Handler onError;
    FrameType requestType;
    RequestPriority priority;
    ResponseStatus callHandler(const Frame &data);
  };

  void queueNotify_(FrameType type, FrameData data) { this->queueRequest_(type, std::move(data), nullptr); }
  void queueRequest_(FrameType type, FrameData data, ResponseHandler onData, Handler onSuccess = nullptr, Handler onError = nullptr, RequestPriority priority = PRIORITY_BACKGROUND);
  void queueRequestPriority_(FrameType type, FrameData data, ResponseHandler onData = nullptr, Handler onSuccess = nullptr, Handler onError = nullptr);
  void sendImmediate(FrameType type, FrameData data, ResponseHandler onData = nullptr, Handler onSuccess = nullptr, Handler onError = nullptr);
  void sendUserCommand(FrameType type, FrameData data, ResponseHandler onData = nullptr, Handler onSuccess = nullptr, Handler onError = nullptr);
  void sendSequencedUserCommand(FrameType type, FrameData data, ResponseHandler onData = nullptr, Handler onSuccess = nullptr, Handler onError = nullptr);
  void cancelCurrentRequest();
  bool shouldSkipPeriodicRequests() const;
  void sendFrame_(FrameType type, const FrameData &data);
  // Setup for appliances
  virtual void setup_() {}
  // Loop for appliances
  virtual void loop_() {}
  /// Calling then ready for request
  virtual void onIdle_() {}
  /// Calling on receiving request
  virtual void onRequest_(const Frame &frame) {}
 private:
  class FrameReceiver : public Frame {
  public:
    bool read(uart::UARTDevice *uart_device);
    void clear() { this->data_.clear(); }
  };
  void sendNetworkNotify_(FrameType msg_type = NETWORK_NOTIFY);
  void handler_(const Frame &frame);
  inline bool isWaitForResponse_() const { return this->request_ != nullptr; }
  void resetAttempts_() { this->remainAttempts_ = this->numAttempts_; }
  void destroyRequest_();
  void resetTimeout_();
  void resetTimeout_(uint32_t customTimeout);
  void sendRequest_(Request *request) { this->sendFrame_(request->requestType, request->request); }
  // Frame receiver with dynamic buffer
  FrameReceiver receiver_{};
  // Network status timer
  Timer networkTimer_{};
  // Waiting response timer
  Timer responseTimer_{};
  // Request period timer
  Timer periodTimer_{};
  // Queue requests
  std::deque<Request *> queue_;
  // Current request
  Request *request_{nullptr};
  // Remaining request attempts
  uint8_t remainAttempts_{};
  // Appliance type
  ApplianceType appType_;
  // Appliance protocol
  uint8_t protocol_{};
  // Period flag
  bool isBusy_{};

  /* ############################## */
  /* ### COMMUNICATION SETTINGS ### */
  /* ############################## */

  // UART device interface
  uart::UARTDevice *uart_device_;
  // Minimal period between requests
  uint32_t period_{1000};
  // Waiting response timeout (default for background requests)
  uint32_t timeout_{2000};
  // User command timeout (shorter for responsiveness, but not too aggressive)
  static constexpr uint32_t USER_COMMAND_TIMEOUT_MS = 1200;
  // Inter-command delay for sequenced user commands
  static constexpr uint32_t INTER_COMMAND_DELAY_MS = 600;
  // Number of request attempts
  uint8_t numAttempts_{3};
};

}  // namespace midea
}  // namespace esphome
