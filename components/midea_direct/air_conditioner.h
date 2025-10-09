#pragma once
#include "appliance_base.h"
#include "capabilities.h"
#include "status_data.h"

namespace esphome {
namespace midea {
namespace ac {

// Air conditioner control command
struct Control {
  Optional<float> targetTemp{};
  Optional<Mode> mode{};
  Optional<Preset> preset{};
  Optional<FanMode> fanMode{};
  Optional<SwingMode> swingMode{};
};

class AirConditioner : public ApplianceBase {
 public:
  AirConditioner() : ApplianceBase(AIR_CONDITIONER) {}
  void setup_() override;
  void onIdle_() override { this->getStatus_(); }
  void control(const Control &control);
  void setPowerState(bool state);
  bool getPowerState() const { return this->mode_ != Mode::MODE_OFF; }
  void togglePowerState() { this->setPowerState(this->mode_ == Mode::MODE_OFF); }
  float getTargetTemp() const { return this->targetTemp_; }
  float getIndoorTemp() const { return this->indoorTemp_; }
  float getOutdoorTemp() const { return this->outdoorTemp_; }
  float getIndoorHum() const { return this->indoorHumidity_; }
  float getPowerUsage() const { return this->powerUsage_; }
  Mode getMode() const { return this->mode_; }
  SwingMode getSwingMode() const { return this->swingMode_; }
  FanMode getFanMode() const { return this->fanMode_; }
  Preset getPreset() const { return this->preset_; }
  const Capabilities &getCapabilities() const { return this->capabilities_; }
  void displayToggle() { this->displayToggle_(); }
 protected:
  void getPowerUsage_();
  void getCapabilities_();
  void getStatus_();
  void setStatus_(StatusData status);
  void displayToggle_();
  ResponseStatus readStatus_(FrameData data);
  Capabilities capabilities_{};
  Timer powerUsageTimer_;
  float indoorHumidity_{};
  float indoorTemp_{};
  float outdoorTemp_{};
  float targetTemp_{};
  float powerUsage_{};
  Mode mode_{Mode::MODE_OFF};
  Preset preset_{Preset::PRESET_NONE};
  FanMode fanMode_{FanMode::FAN_AUTO};
  SwingMode swingMode_{SwingMode::SWING_OFF};
  Preset lastPreset_{Preset::PRESET_NONE};
  StatusData status_{};
  bool sendControl_{};
};

}  // namespace ac
}  // namespace midea
}  // namespace esphome
