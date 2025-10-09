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
    if (length == OFFSET_START && data != START_BYTE)
      continue;
    if (length == OFFSET_LENGTH && data <= OFFSET_DATA) {
      this->data_.clear();
      continue;
    }
    this->data_.push_back(data);
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
  if (this->queue_.empty()) {
    this->onIdle_();
    return;
  }
  this->request_ = this->queue_.front();
  this->queue_.pop_front();
  ESP_LOGD(TAG, "Getting and sending a request from the queue...");
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
  this->responseTimer_.setCallback([this](Timer *timer) {
    ESP_LOGD(TAG, "Response timeout...");
    if (!--this->remainAttempts_) {
      if (this->request_->onError != nullptr)
        this->request_->onError();
      this->destroyRequest_();
      return;
    }
    ESP_LOGD(TAG, "Sending request again. Attempts left: %d...", this->remainAttempts_);
    this->sendRequest_(this->request_);
    this->resetTimeout_();
  });
  this->responseTimer_.start(this->timeout_);
}

void ApplianceBase::destroyRequest_() {
  ESP_LOGD(TAG, "Destroying the request...");
  this->responseTimer_.stop();
  delete this->request_;
  this->request_ = nullptr;
}

void ApplianceBase::sendFrame_(FrameType type, const FrameData &data) {
  Frame frame(this->appType_, this->protocol_, type, data);
  ESP_LOGD(TAG, "TX: %s", frame.toString().c_str());
  this->uart_device_->write_array(frame.data(), frame.size());
  this->isBusy_ = true;
  this->periodTimer_.setCallback([this](Timer *timer) {
    this->isBusy_ = false;
    timer->stop();
  });
  this->periodTimer_.start(this->period_);
}

void ApplianceBase::queueRequest_(FrameType type, FrameData data, ResponseHandler onData, Handler onSuccess, Handler onError) {
  ESP_LOGD(TAG, "Enqueuing the request...");
  this->queue_.push_back(new Request{std::move(data), std::move(onData), std::move(onSuccess), std::move(onError), type});
}

void ApplianceBase::queueRequestPriority_(FrameType type, FrameData data, ResponseHandler onData, Handler onSuccess, Handler onError) {
  ESP_LOGD(TAG, "Priority request queuing...");
  this->queue_.push_front(new Request{std::move(data), std::move(onData), std::move(onSuccess), std::move(onError), type});
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

void ApplianceBase::setBeeper(bool value) {
  ESP_LOGD(TAG, "Turning %s beeper feedback...", value ? "ON" : "OFF");
  this->beeper_ = value;
}

} // namespace midea
} // namespace esphome
