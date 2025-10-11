#include "air_conditioner.h"
#include "timer.h"
#include "esphome/core/log.h"

namespace esphome {
namespace midea {
namespace ac {

static const char *TAG = "AirConditioner";

void AirConditioner::setup_() {
  if (this->autoconf_status_ != AUTOCONF_DISABLED)
    this->getCapabilities_();
  this->timer_manager_.registerTimer(this->powerUsageTimer_);
  this->powerUsageTimer_.setCallback([this](Timer *timer) {
    timer->reset();
    this->getPowerUsage_();
  });
  this->powerUsageTimer_.start(POWER_USAGE_QUERY_INTERVAL_MS);
}

static bool checkConstraints(const Mode &mode, const Preset &preset) {
  if (mode == Mode::MODE_OFF)
    return preset == Preset::PRESET_NONE;
  switch (preset) {
    case Preset::PRESET_NONE:
      return true;
    case Preset::PRESET_ECO:
      return mode == Mode::MODE_COOL;
    case Preset::PRESET_BOOST:
      return mode == Mode::MODE_COOL || mode == Mode::MODE_HEAT;
    case Preset::PRESET_SLEEP:
      return mode != Mode::MODE_DRY && mode != Mode::MODE_FAN_ONLY;
    case Preset::PRESET_AWAY:
      return mode == Mode::MODE_HEAT;
    default:
      return false;
  }
}

void AirConditioner::control(const Control &control) {
  if (this->sendControl_)
    return;

  // Command coalescing: avoid sending duplicate commands too quickly
  uint32_t now = esphome::millis();
  if (now - this->lastCommandTime_ < 100) { // 100ms debounce
    ESP_LOGD(TAG, "Command debounced - too soon after last command");
    return;
  }

  StatusData status = this->status_;
  Mode mode = this->mode_;
  Preset preset = this->preset_;
  bool hasUpdate = false;
  bool isModeChanged = false;
  if (control.mode.has_value() && control.mode.value() != mode) {
    hasUpdate = true;
    isModeChanged = true;
    mode = control.mode.value();
    if (this->mode_ == Mode::MODE_OFF)
      preset = this->lastPreset_;
    else if (!checkConstraints(mode, preset))
      preset = Preset::PRESET_NONE;
  }
  if (control.preset.has_value() && control.preset.value() != preset && checkConstraints(mode, control.preset.value())) {
    hasUpdate = true;
    preset = control.preset.value();
  }
  if (mode != Mode::MODE_OFF) {
    if (mode == Mode::MODE_HEAT_COOL || preset != Preset::PRESET_NONE) {
      if (this->fanMode_ != FanMode::FAN_AUTO) {
        hasUpdate = true;
        status.setFanMode(FanMode::FAN_AUTO);
      }
    } else if (control.fanMode.has_value() && control.fanMode.value() != this->fanMode_) {
      hasUpdate = true;
      status.setFanMode(control.fanMode.value());
    }
    if (control.swingMode.has_value() && control.swingMode.value() != this->swingMode_) {
      hasUpdate = true;
      status.setSwingMode(control.swingMode.value());
    }
  }
  if (control.targetTemp.has_value() && control.targetTemp.value() != this->targetTemp_) {
    hasUpdate = true;
    status.setTargetTemp(control.targetTemp.value());
  }
  if (hasUpdate) {
    this->sendControl_ = true;
    status.setMode(mode);
    status.setPreset(preset);
    status.setBeeper(this->beeper_);
    status.appendCRC();

    // Check if this command is identical to the last one sent recently
    if (this->lastSentCommand_.size() > 0 &&
        status.getMode() == this->lastSentCommand_.getMode() &&
        status.getFanMode() == this->lastSentCommand_.getFanMode() &&
        status.getSwingMode() == this->lastSentCommand_.getSwingMode() &&
        status.getPreset() == this->lastSentCommand_.getPreset() &&
        std::abs(status.getTargetTemp() - this->lastSentCommand_.getTargetTemp()) < 0.1f &&
        now - this->lastCommandTime_ < 2000) { // 2 seconds
      ESP_LOGD(TAG, "Skipping duplicate command - identical to last sent command");
      this->sendControl_ = false;
      return;
    }

    // Update last command tracking
    this->lastSentCommand_ = status;
    this->lastCommandTime_ = now;

    if (isModeChanged && preset != Preset::PRESET_NONE && preset != Preset::PRESET_SLEEP) {
      // Last command with preset
      this->setStatus_(status);
      status.setPreset(Preset::PRESET_NONE);
      status.setBeeper(false);
      status.updateCRC();
      // First command without preset
      this->queueRequestPriority_(FrameType::DEVICE_CONTROL, std::move(status),
        // onData
        std::bind(&AirConditioner::readStatus_, this, std::placeholders::_1)
      );
    } else {
      this->setStatus_(std::move(status));
    }
  }
}

void AirConditioner::setStatus_(StatusData status) {
  ESP_LOGD(TAG, "Sending user command SET_STATUS(0x40) request with high priority...");
  this->sendUserCommand(FrameType::DEVICE_CONTROL, std::move(status),
    // onData
    std::bind(&AirConditioner::readStatus_, this, std::placeholders::_1),
    // onSuccess
    [this]() {
      this->sendControl_ = false;
    },
    // onError
    [this]() {
      ESP_LOGW(TAG, "SET_STATUS(0x40) request failed...");
      this->sendControl_ = false;
    }
  );
}

void AirConditioner::setPowerState(bool state) {
  if (state != this->getPowerState()) {
    Control control;
    control.mode = state ? this->status_.getRawMode() : Mode::MODE_OFF;
    this->control(control);
  }
}

void AirConditioner::getPowerUsage_() {
  QueryPowerData data{};
  ESP_LOGD(TAG, "Enqueuing a GET_POWERUSAGE(0x41) request...");
  this->queueRequest_(FrameType::DEVICE_QUERY, std::move(data),
    // onData
    [this](FrameData data) -> ResponseStatus {
      const auto status = data.to<StatusData>();
      if (!status.hasPowerInfo())
        return ResponseStatus::RESPONSE_WRONG;
      if (this->powerUsage_ != status.getPowerUsage()) {
        this->powerUsage_ = status.getPowerUsage();
        this->sendUpdate();
      }
      return ResponseStatus::RESPONSE_OK;
    },
    nullptr, nullptr, PRIORITY_BACKGROUND
  );
}

void AirConditioner::getCapabilities_() {
  GetCapabilitiesData data{};
  this->autoconf_status_ = AUTOCONF_PROGRESS;
  ESP_LOGD(TAG, "Enqueuing a priority GET_CAPABILITIES(0xB5) request...");
  this->queueRequest_(FrameType::DEVICE_QUERY, std::move(data),
    // onData
    [this](FrameData data) -> ResponseStatus {
      if (!data.hasID(0xB5))
        return ResponseStatus::RESPONSE_WRONG;
      if (this->capabilities_.read(data)) {
        GetCapabilitiesSecondData data{};
        this->sendFrame_(FrameType::DEVICE_QUERY, data);
        return ResponseStatus::RESPONSE_PARTIAL;
      }
      return ResponseStatus::RESPONSE_OK;
    },
    // onSuccess
    [this]() {
      this->autoconf_status_ = AUTOCONF_OK;
    },
    // onError
    [this]() {
      ESP_LOGW(TAG, "Failed to get 0xB5 capabilities report.");
      this->autoconf_status_ = AUTOCONF_ERROR;
    },
    PRIORITY_BACKGROUND
  );
}

void AirConditioner::getStatus_() {
  QueryStateData data{};
  ESP_LOGD(TAG, "Enqueuing a GET_STATUS(0x41) request...");
  this->queueRequest_(FrameType::DEVICE_QUERY, std::move(data),
    // onData
    std::bind(&AirConditioner::readStatus_, this, std::placeholders::_1),
    nullptr, nullptr, PRIORITY_BACKGROUND
  );
}

void AirConditioner::displayToggle_() {
  DisplayToggleData data{};
  ESP_LOGD(TAG, "Enqueuing a priority TOGGLE_LIGHT(0x41) request...");
  this->queueRequest_(FrameType::DEVICE_QUERY, std::move(data),
    // onData
    std::bind(&AirConditioner::readStatus_, this, std::placeholders::_1)
  );
}

template<typename T>
void setProperty(T &property, const T &value, bool &update) {
  if (property != value) {
    property = value;
    update = true;
  }
}

ResponseStatus AirConditioner::readStatus_(FrameData data) {
  if (!data.hasStatus())
    return ResponseStatus::RESPONSE_WRONG;
  ESP_LOGD(TAG, "New status data received. Parsing...");
  bool hasUpdate = false;
  const StatusData newStatus = data.to<StatusData>();
  this->status_.copyStatus(newStatus);
  if (this->mode_ != newStatus.getMode()) {
    hasUpdate = true;
    this->mode_ = newStatus.getMode();
    if (newStatus.getMode() == Mode::MODE_OFF)
      this->lastPreset_ = this->preset_;
  }
  setProperty(this->preset_, newStatus.getPreset(), hasUpdate);
  setProperty(this->fanMode_, newStatus.getFanMode(), hasUpdate);
  setProperty(this->swingMode_, newStatus.getSwingMode(), hasUpdate);
  setProperty(this->targetTemp_, newStatus.getTargetTemp(), hasUpdate);
  setProperty(this->indoorTemp_, newStatus.getIndoorTemp(), hasUpdate);
  setProperty(this->outdoorTemp_, newStatus.getOutdoorTemp(), hasUpdate);
  setProperty(this->indoorHumidity_, newStatus.getHumiditySetpoint(), hasUpdate);
  if (hasUpdate)
    this->sendUpdate();
  return ResponseStatus::RESPONSE_OK;
}

}  // namespace ac
}  // namespace midea
}  // namespace esphome
