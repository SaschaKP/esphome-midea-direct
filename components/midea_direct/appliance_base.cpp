#include "esphome/components/wifi/wifi_component.h"
#include "esphome/components/network/util.h"
#include "esphome/core/util.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "appliance_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace midea {

static const char *TAG = "ApplianceBase";

ResponseStatus ApplianceBase::Request::callHandler(const Frame &frame) {
  if (!frame.hasType(this->requestType))
    return ResponseStatus::RESPONSE_WRONG;
  if (this->onData == nullptr)
    return RESPONSE_OK;
  return this->onData(frame.getData());
}

bool ApplianceBase::FrameReceiver::read(uart::UARTDevice *uart_device) {
  while (uart_device->available()) {
    uint8_t data;
    if (!uart_device->read_byte(&data)) {
      break;
    }
    const uint8_t length = this->data_.size();

    // Skip invalid start bytes
    if (length == OFFSET_START && data != START_BYTE)
      continue;

    // Skip invalid length bytes
    if (length == OFFSET_LENGTH && data <= OFFSET_DATA) {
      this->data_.clear();
      continue;
    }

    this->data_.push_back(data);

    // Check if we have a complete frame
    if (length > OFFSET_DATA && length >= this->data_[OFFSET_LENGTH]) {
      if (this->isValid())
        return true;
      this->data_.clear();
    }
  }
  return false;
}

void ApplianceBase::setup() {
  this->timer_manager_.registerTimer(this->periodTimer_);
  this->timer_manager_.registerTimer(this->networkTimer_);
  this->timer_manager_.registerTimer(this->responseTimer_);
  this->networkTimer_.setCallback([this](Timer *timer) {
    this->sendNetworkNotify_();
    timer->reset();
  });
  this->networkTimer_.start(2 * 60 * 1000);
  this->networkTimer_.call();
  this->setup_();
}

void ApplianceBase::loop() {
  // Timers task
  timer_manager_.task();
  // Loop for appliances
  loop_();
  // Frame receiving
  while (this->receiver_.read(this->uart_device_)) {
    this->protocol_ = this->receiver_.getProtocol();
    ESP_LOGD(TAG, "RX: %s", this->receiver_.toString().c_str());
    this->handler_(this->receiver_);
    this->receiver_.clear();
  }
  if (this->isBusy_ || this->isWaitForResponse_())
    return;

  // Check if we have sequenced commands waiting
  if (!this->queue_.empty() && this->is_in_sequence_mode_) {
    // Check if enough time has passed for next sequenced command
    uint32_t now = esphome::millis();
    uint32_t time_since_last = now - this->last_sequence_command_time_;
    if (time_since_last >= INTER_COMMAND_DELAY_MS) {
      ESP_LOGD(TAG, "Sequence delay satisfied, processing next sequenced command...");
      this->is_in_sequence_mode_ = false; // Allow processing
    } else {
      // Still waiting for sequence delay, skip processing
      ESP_LOGV(TAG, "Waiting for sequence delay (%d/%d ms)...", time_since_last, INTER_COMMAND_DELAY_MS);
      return;
    }
  }

  if (this->queue_.empty()) {
    // Skip periodic requests if we have pending user commands
    if (!this->shouldSkipPeriodicRequests()) {
      this->onIdle_();
    }
    return;
  }

  // Get next request from queue
  this->request_ = this->queue_.front();
  this->queue_.pop_front();

  // Handle sequenced commands specially
  if (this->request_->priority == PRIORITY_USER_SEQUENCE) {
    ESP_LOGD(TAG, "Processing sequenced user command...");
    this->last_sequence_command_time_ = esphome::millis();
    this->is_in_sequence_mode_ = true; // Set flag for next command delay
  } else {
    ESP_LOGD(TAG, "Getting and sending a request from the queue...");
  }

  this->sendRequest_(this->request_);
  if (this->request_->onData != nullptr) {
    this->resetAttempts_();
    this->resetTimeout_();
  } else {
    this->destroyRequest_();
  }
}

void ApplianceBase::handler_(const Frame &frame) {
  if (this->isWaitForResponse_()) {
    auto result = this->request_->callHandler(frame);
    if (result != RESPONSE_WRONG) {
      if (result == RESPONSE_OK) {
        if (this->request_->onSuccess != nullptr)
          this->request_->onSuccess();
        this->destroyRequest_();
      } else {
        this->resetAttempts_();
        this->resetTimeout_();
      }
      return;
    }
  }
  // ignoring responses on network notifies
  if (frame.hasType(NETWORK_NOTIFY))
    return;
  /* HANDLE REQUESTS */
  if (frame.hasType(QUERY_NETWORK)) {
    this->sendNetworkNotify_(QUERY_NETWORK);
    return;
  }
  this->onRequest_(frame);
}

static uint8_t getSignalStrength() {
#ifdef USE_WIFI
  // Try to get WiFi signal strength if available
    if (esphome::network::is_connected() && esphome::wifi::global_wifi_component != nullptr) {
      int8_t rssi = esphome::wifi::global_wifi_component->wifi_rssi();

      // Convert RSSI to 1-5 scale based on typical WiFi strength indicators
      if (rssi >= -50) return 5;      // Excellent: ≥-50dBm
      else if (rssi >= -60) return 4; // Good: ≥-60dBm
      else if (rssi >= -70) return 3; // Fair: ≥-70dBm
      else if (rssi >= -80) return 2; // Poor: ≥-80dBm
      else return 1;                  // Weak: <-80dBm
    }
#endif
  return 3; // Return default signal strength for RTL87xx or when WiFi unavailable
}

void ApplianceBase::sendNetworkNotify_(FrameType msgType) {
#ifdef USE_NETWORK
  esphome::network::IPAddresses ip_addresses = esphome::network::get_ip_addresses();
  esphome::network::IPAddress real_ip;
  if (!ip_addresses.empty()) {
    real_ip = ip_addresses[0]; // Use first IP address
  }
  
  // Extract IP address bytes from string representation (most compatible method)
  std::string ip_str = real_ip.str();
#endif
  uint8_t ip_byte1 = 192, ip_byte2 = 168, ip_byte3 = 1, ip_byte4 = 100; // Default fallback
  
#ifdef USE_NETWORK
  // Parse IP string "X.X.X.X" to extract bytes
  if (!ip_str.empty() && ip_str != "0.0.0.0") {
    int ip1, ip2, ip3, ip4;
    if (sscanf(ip_str.c_str(), "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) == 4) {
      ip_byte1 = (uint8_t)ip1;
      ip_byte2 = (uint8_t)ip2;
      ip_byte3 = (uint8_t)ip3;
      ip_byte4 = (uint8_t)ip4;
    }
  }
#endif
  
  NetworkNotifyData notify{};
#ifdef USE_NETWORK
  notify.setConnected(esphome::network::is_connected());
#else
  notify.setConnected(true); // Assume connected for RTL87xx or when network unavailable
#endif
  notify.setSignalStrength(getSignalStrength());
  notify.setIP(ip_byte1, ip_byte2, ip_byte3, ip_byte4);
  notify.appendCRC();
  if (msgType == NETWORK_NOTIFY) {
    ESP_LOGD(TAG, "Enqueuing a DEVICE_NETWORK(0x0D) notification...");
    this->queueNotify_(msgType, std::move(notify));
  } else {
    ESP_LOGD(TAG, "Answer to QUERY_NETWORK(0x63) request...");
    this->sendFrame_(msgType, std::move(notify));
  }
}

void ApplianceBase::resetTimeout_() {
  this->resetTimeout_(this->timeout_);
}

void ApplianceBase::resetTimeout_(uint32_t customTimeout) {
  this->responseTimer_.setCallback([this, customTimeout](Timer *timer) {
    ESP_LOGD(TAG, "Response timeout...");
    if (!--this->remainAttempts_) {
      if (this->request_->onError != nullptr)
        this->request_->onError();
      this->destroyRequest_();
      return;
    }
    ESP_LOGD(TAG, "Sending request again. Attempts left: %d...", this->remainAttempts_);
    // For user commands, use exponential backoff to avoid overwhelming the AC
    uint32_t retryDelay = customTimeout;
    if (this->has_pending_user_command_) {
      retryDelay = std::min(customTimeout * 2, 3000u); // Cap at 3 seconds
    }
    this->sendRequest_(this->request_);
    this->resetTimeout_(retryDelay);
  });
  this->responseTimer_.start(customTimeout);
}

void ApplianceBase::destroyRequest_() {
  ESP_LOGD(TAG, "Destroying the request...");
  this->responseTimer_.stop();
  delete this->request_;
  this->request_ = nullptr;
  // Reset user command flag when request is destroyed
  this->has_pending_user_command_ = false;

  // Check if we need to schedule next sequenced command
  if (this->is_in_sequence_mode_ && !this->queue_.empty()) {
    uint32_t now = esphome::millis();
    uint32_t time_since_last_command = now - this->last_sequence_command_time_;

    if (time_since_last_command >= INTER_COMMAND_DELAY_MS) {
      // Enough time has passed, schedule next command immediately
      ESP_LOGD(TAG, "Sequence delay satisfied, scheduling next command...");
      this->is_in_sequence_mode_ = false; // Reset for next command
      // The loop will pick up the next command naturally
    } else {
      // Schedule timer for remaining delay
      uint32_t remaining_delay = INTER_COMMAND_DELAY_MS - time_since_last_command;
      ESP_LOGD(TAG, "Scheduling next sequenced command in %d ms...", remaining_delay);

      // Use a temporary timer for sequence delay
      static Timer sequenceTimer;
      this->timer_manager_.registerTimer(sequenceTimer);
      sequenceTimer.setCallback([this](Timer *timer) {
        ESP_LOGD(TAG, "Sequence delay timer fired, enabling next command...");
        this->is_in_sequence_mode_ = false;
        timer->stop();
      });
      sequenceTimer.start(remaining_delay);
    }
  }
}

void ApplianceBase::sendFrame_(FrameType type, const FrameData &data) {
  Frame frame(this->appType_, this->protocol_, type, data);
  ESP_LOGD(TAG, "TX: %s", frame.toString().c_str());
  this->uart_device_->write_array(frame.data(), frame.size());
  this->isBusy_ = true;
  // Reduce busy period for user commands to improve responsiveness
  uint32_t busyPeriod = (this->has_pending_user_command_) ? (this->period_ / 2) : this->period_;
  this->periodTimer_.setCallback([this](Timer *timer) {
    this->isBusy_ = false;
    timer->stop();
  });
  this->periodTimer_.start(busyPeriod);
}

void ApplianceBase::queueRequest_(FrameType type, FrameData data, ResponseHandler onData, Handler onSuccess, Handler onError, RequestPriority priority) {
  ESP_LOGD(TAG, "Enqueuing the request...");
  this->queue_.push_back(new Request{std::move(data), std::move(onData), std::move(onSuccess), std::move(onError), type, priority});
}

void ApplianceBase::queueRequestPriority_(FrameType type, FrameData data, ResponseHandler onData, Handler onSuccess, Handler onError) {
  ESP_LOGD(TAG, "Priority request queuing...");
  this->queue_.push_front(new Request{std::move(data), std::move(onData), std::move(onSuccess), std::move(onError), type, PRIORITY_USER_COMMAND});
}

void ApplianceBase::sendImmediate(FrameType type, FrameData data, ResponseHandler onData, Handler onSuccess, Handler onError) {
  if (!isBusy_ && queue_.empty()) {
    ESP_LOGD(TAG, "Sending immediate request...");
    Request *req = new Request{std::move(data), std::move(onData), std::move(onSuccess), std::move(onError), type};
    sendRequest_(req);
    if (req->onData != nullptr) {
      resetAttempts_();
      resetTimeout_();
      request_ = req;
    } else {
      delete req;
    }
  } else {
    ESP_LOGD(TAG, "Queuing priority request (not immediate)...");
    queueRequestPriority_(type, std::move(data), std::move(onData), std::move(onSuccess), std::move(onError));
  }
}

void ApplianceBase::sendUserCommand(FrameType type, FrameData data, ResponseHandler onData, Handler onSuccess, Handler onError) {
  // Mark that we have a pending user command
  has_pending_user_command_ = true;
  last_user_command_time_ = esphome::millis();

  // Cancel any current non-user request to prioritize user command
  if (isWaitForResponse_() && request_ != nullptr) {
    ESP_LOGD(TAG, "Cancelling current request for user command priority...");
    cancelCurrentRequest();
  }

  // Clear any queued background requests to prioritize user command
  auto it = queue_.begin();
  while (it != queue_.end()) {
    if ((*it)->priority == PRIORITY_BACKGROUND) {
      ESP_LOGD(TAG, "Removing background request from queue for user command priority");
      delete *it;
      it = queue_.erase(it);
    } else {
      ++it;
    }
  }

  // Improved immediate sending logic: check if we can send immediately even if busy with background tasks
  bool canSendImmediately = !isBusy_ || (isWaitForResponse_() && request_ == nullptr);

  if (canSendImmediately) {
    ESP_LOGD(TAG, "Sending user command immediately...");
    Request *req = new Request{std::move(data), std::move(onData), std::move(onSuccess), std::move(onError), type};
    sendRequest_(req);
    if (req->onData != nullptr) {
      resetAttempts_();
      // Use shorter timeout for user commands
      resetTimeout_(USER_COMMAND_TIMEOUT_MS);
      request_ = req;
    } else {
      delete req;
    }
  } else {
    ESP_LOGD(TAG, "Queuing user command with priority...");
    queueRequestPriority_(type, std::move(data), std::move(onData), std::move(onSuccess), std::move(onError));
  }
}

void ApplianceBase::sendSequencedUserCommand(FrameType type, FrameData data, ResponseHandler onData, Handler onSuccess, Handler onError) {
  uint32_t now = esphome::millis();

  // If this is the first command in a sequence, initialize sequence tracking
  if (!is_in_sequence_mode_) {
    is_in_sequence_mode_ = true;
    sequence_start_time_ = now;
    last_sequence_command_time_ = 0; // Allow immediate first command
    ESP_LOGD(TAG, "Starting new user command sequence...");
  }

  // Check if enough time has passed since last sequenced command
  uint32_t time_since_last = now - last_sequence_command_time_;
  if (time_since_last >= INTER_COMMAND_DELAY_MS) {
    // Enough time has passed, send immediately
    ESP_LOGD(TAG, "Sending sequenced command immediately (delay satisfied)...");
    last_sequence_command_time_ = now;

    // Use priority queuing for sequenced commands
    queueRequest_(type, std::move(data), std::move(onData), std::move(onSuccess), std::move(onError), PRIORITY_USER_SEQUENCE);
  } else {
    // Not enough time, queue with sequence priority
    ESP_LOGD(TAG, "Queuing sequenced command (waiting for delay)...");
    queueRequest_(type, std::move(data), std::move(onData), std::move(onSuccess), std::move(onError), PRIORITY_USER_SEQUENCE);
  }
}

void ApplianceBase::cancelCurrentRequest() {
  if (request_ != nullptr) {
    ESP_LOGD(TAG, "Cancelling current request...");
    responseTimer_.stop();
    delete request_;
    request_ = nullptr;
    remainAttempts_ = 0;
  }
}

bool ApplianceBase::shouldSkipPeriodicRequests() const {
  // Skip periodic requests if we have a recent user command pending or in sequence mode
  bool has_recent_user_command = has_pending_user_command_ &&
         (esphome::millis() - last_user_command_time_) < 5000; // 5 seconds grace period

  // Also skip if we're in sequence mode (processing sequenced commands)
  bool in_sequence = is_in_sequence_mode_ ||
         (!queue_.empty() && std::any_of(queue_.begin(), queue_.end(),
           [](const Request* req) { return req->priority == PRIORITY_USER_SEQUENCE; }));

  return has_recent_user_command || in_sequence;
}

void ApplianceBase::setBeeper(bool value) {
  ESP_LOGD(TAG, "Turning %s beeper feedback...", value ? "ON" : "OFF");
  this->beeper_ = value;
}

} // namespace midea
} // namespace esphome
