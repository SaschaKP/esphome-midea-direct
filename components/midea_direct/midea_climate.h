#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "air_conditioner.h"
#include <set>

namespace esphome {
namespace midea_direct {

// ESPHome climate wrapper for MideaUART_v2 AirConditioner
class MideaClimate : public climate::Climate, public Component, public uart::UARTDevice, public esphome::midea::ac::AirConditioner {
 public:
  void setup() override;
  void loop() override;
  
  // ESPHome climate interface
  void control(const climate::ClimateCall& call) override;
  climate::ClimateTraits traits() override;
  void dump_config() override;
  
  // Configuration setters
  void set_supported_modes(const std::vector<climate::ClimateMode>& modes);
  void set_supported_fan_modes(const std::vector<climate::ClimateFanMode>& modes);
  void set_supported_swing_modes(const std::vector<climate::ClimateSwingMode>& modes);
  void set_supported_presets(const std::vector<climate::ClimatePreset>& presets);
  
  // Custom mode setters
  void set_custom_fan_modes(const std::vector<std::string>& modes);
  void set_custom_presets(const std::vector<std::string>& presets);
  
  // Sensor setters
  void set_power_sensor(sensor::Sensor* sensor) { power_sensor_ = sensor; }
  void set_outdoor_temperature_sensor(sensor::Sensor* sensor) { outdoor_temperature_sensor_ = sensor; }
  void set_indoor_humidity_sensor(sensor::Sensor* sensor) { indoor_humidity_sensor_ = sensor; }
  
  // ApplianceBase configuration interface - using proper public methods
  void set_period(uint32_t period) { this->setPeriod(period); }
  void set_timeout(uint32_t timeout) { this->setTimeout(timeout); }
  void set_num_attempts(uint8_t attempts) { this->setNumAttempts(attempts); }
  void set_autoconf(bool autoconf) { 
    this->setAutoconf(autoconf);
  }
  void set_beeper_config(bool beeper) { this->setBeeper(beeper); }
  
  // UART device setup
  void setup_uart_device() {
    this->setUARTDevice(this); // Set the UART device directly for MideaUART_v2
  }
  
 protected:
  // Override MideaUART_v2 virtual methods for debugging
  void setup_() override;
  void loop_() override;
  void onIdle_() override;
  
  // Override sendUpdate from ApplianceBase to update ESPHome state
  void sendUpdate();
  
  // ESPHome configuration
  std::set<climate::ClimateMode> supported_modes_;
  std::set<climate::ClimateFanMode> supported_fan_modes_;
  std::set<climate::ClimateSwingMode> supported_swing_modes_;
  std::set<climate::ClimatePreset> supported_presets_;
  
  // Custom mode configuration
  std::vector<std::string> custom_fan_modes_;
  std::vector<std::string> custom_presets_;
  
  // ESPHome sensors
  sensor::Sensor* power_sensor_ = nullptr;
  sensor::Sensor* outdoor_temperature_sensor_ = nullptr;
  sensor::Sensor* indoor_humidity_sensor_ = nullptr;
  
 private:
  // Setup state tracking
  bool setup_complete_ = false;
  
  // Control tracking to prevent redundant updates
  uint32_t last_control_time_ = 0;
  
  
  // Update ESPHome state from MideaUART_v2 state
  void update_esphome_state();
};

}  // namespace midea_direct
}  // namespace esphome
