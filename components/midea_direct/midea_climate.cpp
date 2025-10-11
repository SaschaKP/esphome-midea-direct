#include "midea_climate.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include "status_data.h"

namespace esphome {
namespace midea_direct {

static const char* const TAG = "midea_climate";

void MideaClimate::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Midea climate...");
  
  // Initialize UART device directly
  setup_uart_device();
  ESP_LOGD(TAG, "UART device initialized");
  
  // Initialize ESPHome climate defaults
  if (supported_modes_.empty()) {
    supported_modes_ = {
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_AUTO,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_FAN_ONLY
    };
  }

  if (supported_fan_modes_.empty()) {
    supported_fan_modes_ = {
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH
    };
  }

  if (supported_swing_modes_.empty()) {
    supported_swing_modes_ = {
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL,
      climate::CLIMATE_SWING_HORIZONTAL,
      climate::CLIMATE_SWING_BOTH
    };
  }

  // FORCE ensure SWING_OFF is always available, regardless of autoconf
  if (std::find(supported_swing_modes_.begin(), supported_swing_modes_.end(), climate::CLIMATE_SWING_OFF) == supported_swing_modes_.end()) {
    supported_swing_modes_.push_back(climate::CLIMATE_SWING_OFF);
    ESP_LOGCONFIG(TAG, "Force-added CLIMATE_SWING_OFF to ensure disable option is available");
  }

  if (supported_presets_.empty()) {
    supported_presets_ = {
      climate::CLIMATE_PRESET_NONE,
      climate::CLIMATE_PRESET_ECO,
      climate::CLIMATE_PRESET_SLEEP
    };
  }

  // FORCE ensure PRESET_NONE is always available, regardless of autoconf
  if (std::find(supported_presets_.begin(), supported_presets_.end(), climate::CLIMATE_PRESET_NONE) == supported_presets_.end()) {
    supported_presets_.push_back(climate::CLIMATE_PRESET_NONE);
    ESP_LOGCONFIG(TAG, "Force-added CLIMATE_PRESET_NONE to ensure disable option is available");
  }
  
  // First call ApplianceBase::setup() which calls our setup_() override
  ApplianceBase::setup();
  
  ESP_LOGD(TAG, "MideaUART_v2 initialization complete");
  
  // Set initial ESPHome state
  update_esphome_state();
  
  // Mark setup as complete to enable state change detection
  setup_complete_ = true;
  ESP_LOGD(TAG, "MideaClimate setup completed");
}

void MideaClimate::loop() {
  // Run main loop
  ApplianceBase::loop();

  // Periodically log status for debugging (every 30 seconds)
  static uint32_t last_debug = 0;
  uint32_t now = esphome::millis();
  if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG && now - last_debug > DEBUG_LOG_INTERVAL_MS) {
    ESP_LOGD(TAG, "Status: mode=%d, temp=%.1f, indoor=%.1f",
             static_cast<int>(this->getMode()), this->getTargetTemp(), this->getIndoorTemp());
    last_debug = now;
  }
}

void MideaClimate::control(const climate::ClimateCall& call) {
  ESP_LOGD(TAG, "Climate control called");
  
  // Create MideaUART_v2 Control structure - using exact namespace and type
  esphome::midea::ac::Control control;
  bool has_control = false;
  
  // Handle mode changes
  if (call.get_mode().has_value()) {
    esphome::midea::ac::Mode midea_mode = this->esphome_mode_to_midea(call.get_mode().value());
    if (midea_mode != this->getMode()) {
      control.mode = midea_mode;
      has_control = true;
      ESP_LOGD(TAG, "Setting mode to %d", static_cast<int>(midea_mode));
    }
  }
  
  // Handle temperature changes
  if (call.get_target_temperature().has_value()) {
    float new_temp = call.get_target_temperature().value();
    if (std::abs(new_temp - this->getTargetTemp()) > 0.1f) {
      control.targetTemp = new_temp;
      has_control = true;
      ESP_LOGD(TAG, "Setting target temperature to %.1f", new_temp);
    }
  }
  
  // Handle fan mode changes
  if (call.get_fan_mode().has_value()) {
    esphome::midea::ac::FanMode midea_fan = this->esphome_fan_to_midea(call.get_fan_mode().value());
    if (midea_fan != this->getFanMode()) {
      control.fanMode = midea_fan;
      has_control = true;
      ESP_LOGD(TAG, "Setting fan mode to %d", static_cast<int>(midea_fan));
    }
  }
  
  // Handle swing mode changes
  if (call.get_swing_mode().has_value()) {
    esphome::midea::ac::SwingMode midea_swing = this->esphome_swing_to_midea(call.get_swing_mode().value());
    if (midea_swing != this->getSwingMode()) {
      control.swingMode = midea_swing;
      has_control = true;
      ESP_LOGD(TAG, "Setting swing mode to %d", static_cast<int>(midea_swing));
    }
  }

  // Handle preset changes
  if (call.get_preset().has_value()) {
    esphome::midea::ac::Preset midea_preset = this->esphome_preset_to_midea(call.get_preset().value());
    if (midea_preset != this->getPreset()) {
      control.preset = midea_preset;
      has_control = true;
      ESP_LOGD(TAG, "Setting preset to %d", static_cast<int>(midea_preset));
    }
  }
  
  // Handle custom fan mode changes
  std::string requested_custom_fan;
  if (call.get_custom_fan_mode().has_value()) {
    requested_custom_fan = call.get_custom_fan_mode().value();
    // Convert custom fan mode to Midea fan mode
    esphome::midea::ac::FanMode midea_fan;
    if (requested_custom_fan == "SILENT") {
      midea_fan = esphome::midea::ac::FanMode::FAN_SILENT;
    } else if (requested_custom_fan == "TURBO") {
      midea_fan = esphome::midea::ac::FanMode::FAN_TURBO;
    } else {
      midea_fan = esphome::midea::ac::FanMode::FAN_AUTO; // Fallback
    }
    if (midea_fan != this->getFanMode()) {
      control.fanMode = midea_fan;
      has_control = true;
      ESP_LOGD(TAG, "Setting custom fan mode '%s' to %d", requested_custom_fan.c_str(), static_cast<int>(midea_fan));
    }
  }

  // Handle custom preset changes
  std::string requested_custom_preset;
  if (call.get_custom_preset().has_value()) {
    requested_custom_preset = call.get_custom_preset().value();
    // Convert custom preset to Midea preset
    esphome::midea::ac::Preset midea_preset;
    if (requested_custom_preset == "FREEZE_PROTECTION") {
      midea_preset = esphome::midea::ac::Preset::PRESET_AWAY;
    } else {
      midea_preset = esphome::midea::ac::Preset::PRESET_NONE; // Fallback
    }
    if (midea_preset != this->getPreset()) {
      control.preset = midea_preset;
      has_control = true;
      ESP_LOGD(TAG, "Setting custom preset '%s' to %d", requested_custom_preset.c_str(), static_cast<int>(midea_preset));
    }
  }
  
  if (has_control) {
    this->AirConditioner::control(control);
    
    // IMMEDIATE UI UPDATE: Update ESPHome state immediately
    // periodic status updates will correct any discrepancies later
    bool ui_update_needed = false;
    
    if (control.mode.has_value()) {
      climate::ClimateMode new_esphome_mode = this->midea_mode_to_esphome(control.mode.value());
      if (this->mode != new_esphome_mode) {
        this->mode = new_esphome_mode;
        ui_update_needed = true;
        ESP_LOGD(TAG, "Immediate UI update: mode -> %d", static_cast<int>(new_esphome_mode));
      }
    }

    if (control.targetTemp.has_value()) {
      float new_target = control.targetTemp.value();
      if (std::abs(this->target_temperature - new_target) > 0.1f) {
        this->target_temperature = new_target;
        ui_update_needed = true;
        ESP_LOGD(TAG, "Immediate UI update: target temp -> %.1f", new_target);
      }
    }

    if (control.fanMode.has_value()) {
      climate::ClimateFanMode new_esphome_fan = this->midea_fan_to_esphome(control.fanMode.value());
      if (this->fan_mode != new_esphome_fan) {
        this->fan_mode = new_esphome_fan;
        ui_update_needed = true;
        ESP_LOGD(TAG, "Immediate UI update: fan mode -> %d", static_cast<int>(new_esphome_fan));
      }
    }

    if (control.swingMode.has_value()) {
      climate::ClimateSwingMode new_esphome_swing = this->midea_swing_to_esphome(control.swingMode.value());
      if (this->swing_mode != new_esphome_swing) {
        this->swing_mode = new_esphome_swing;
        ui_update_needed = true;
        ESP_LOGD(TAG, "Immediate UI update: swing mode -> %d", static_cast<int>(new_esphome_swing));
      }
    }

    if (control.preset.has_value()) {
      climate::ClimatePreset new_esphome_preset = this->midea_preset_to_esphome(control.preset.value());
      if (this->preset != new_esphome_preset) {
        this->preset = new_esphome_preset;
        ui_update_needed = true;
        ESP_LOGD(TAG, "Immediate UI update: preset -> %d", static_cast<int>(new_esphome_preset));
      }
    }
    
    // Handle immediate UI updates for custom modes
    if (!requested_custom_fan.empty()) {
      // For custom fan modes, we also need to update the standard fan_mode field
      // The ESPHome UI will show the custom fan mode name
      if (control.fanMode.has_value()) {
        climate::ClimateFanMode new_esphome_fan = this->midea_fan_to_esphome(control.fanMode.value());
        if (this->fan_mode != new_esphome_fan) {
          this->fan_mode = new_esphome_fan;
          ui_update_needed = true;
          ESP_LOGD(TAG, "Immediate UI update: custom fan '%s' -> standard fan %d",
                   requested_custom_fan.c_str(), static_cast<int>(new_esphome_fan));
        }
      }
    }

    if (!requested_custom_preset.empty()) {
      // For custom presets, we also need to update the standard preset field
      if (control.preset.has_value()) {
        climate::ClimatePreset new_esphome_preset = this->midea_preset_to_esphome(control.preset.value());
        if (this->preset != new_esphome_preset) {
          this->preset = new_esphome_preset;
          ui_update_needed = true;
          ESP_LOGD(TAG, "Immediate UI update: custom preset '%s' -> standard preset %d",
                   requested_custom_preset.c_str(), static_cast<int>(new_esphome_preset));
        }
      }
    }
    
    // Publish immediately to Home Assistant for responsive UI
    if (ui_update_needed) {
      this->publish_state();
      last_control_time_ = esphome::millis();  // Track when we sent immediate update
      ESP_LOGD(TAG, "Immediate UI update published to Home Assistant (will be verified by status updates)");
    }
  }
}

climate::ClimateTraits MideaClimate::traits() {
  // ESPHome should call this method only once during setup, not repeatedly!
  // Log only once to avoid spam
  static bool traits_logged = false;
  if (!traits_logged) {
    ESP_LOGD(TAG, "ESPHome requesting climate traits (should only happen once)");
    ESP_LOGD(TAG, "Adding %d supported swing modes to traits:", supported_swing_modes_.size());
    for (auto swing_mode : supported_swing_modes_) {
      const char* mode_name;
      switch (swing_mode) {
        case climate::CLIMATE_SWING_OFF: mode_name = "OFF"; break;
        case climate::CLIMATE_SWING_VERTICAL: mode_name = "VERTICAL"; break;
        case climate::CLIMATE_SWING_HORIZONTAL: mode_name = "HORIZONTAL"; break;
        case climate::CLIMATE_SWING_BOTH: mode_name = "BOTH"; break;
        default: mode_name = "UNKNOWN"; break;
      }
      ESP_LOGD(TAG, "  - Swing mode: %s (%d)", mode_name, static_cast<int>(swing_mode));
    }
    ESP_LOGD(TAG, "Adding %d supported presets to traits:", supported_presets_.size());
    for (auto preset : supported_presets_) {
      const char* preset_name;
      switch (preset) {
        case climate::CLIMATE_PRESET_NONE: preset_name = "NONE"; break;
        case climate::CLIMATE_PRESET_ECO: preset_name = "ECO"; break;
        case climate::CLIMATE_PRESET_AWAY: preset_name = "AWAY"; break;
        case climate::CLIMATE_PRESET_BOOST: preset_name = "BOOST"; break;
        case climate::CLIMATE_PRESET_COMFORT: preset_name = "COMFORT"; break;
        case climate::CLIMATE_PRESET_HOME: preset_name = "HOME"; break;
        case climate::CLIMATE_PRESET_SLEEP: preset_name = "SLEEP"; break;
        case climate::CLIMATE_PRESET_ACTIVITY: preset_name = "ACTIVITY"; break;
        default: preset_name = "UNKNOWN"; break;
      }
      ESP_LOGD(TAG, "  - Preset: %s (%d)", preset_name, static_cast<int>(preset));
    }
    traits_logged = true;
  }
  
  auto traits = climate::ClimateTraits();
  
  // Set supported modes
  for (auto mode : supported_modes_) {
    traits.add_supported_mode(mode);
  }
  
  // Set supported fan modes
  for (auto fan_mode : supported_fan_modes_) {
    traits.add_supported_fan_mode(fan_mode);
  }
  
  // Set supported swing modes
  for (auto swing_mode : supported_swing_modes_) {
    traits.add_supported_swing_mode(swing_mode);
  }
  
  // Set supported presets
  for (auto preset : supported_presets_) {
    traits.add_supported_preset(preset);
  }
  
  for (const auto& custom_fan_mode : custom_fan_modes_) {
    traits.add_supported_custom_fan_mode(custom_fan_mode);
  }
  
  for (const auto& custom_preset : custom_presets_) {
    traits.add_supported_custom_preset(custom_preset);
  }
  
  // Temperature range exactly as MideaUART_v2
  traits.set_supports_current_temperature(true);
  traits.set_visual_min_temperature(16.0f);
  traits.set_visual_max_temperature(31.0f);
  traits.set_visual_temperature_step(1.0f);
  
  return traits;
}

void MideaClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Midea Climate:");
  LOG_CLIMATE("", "Midea Climate", this);
  ESP_LOGCONFIG(TAG, "  Period: %d ms", this->getPeriod());
  ESP_LOGCONFIG(TAG, "  Timeout: %d ms", this->getTimeout());
  ESP_LOGCONFIG(TAG, "  Max attempts: %d", this->getNumAttempts());
  ESP_LOGCONFIG(TAG, "  Autoconf status: %d", static_cast<int>(this->getAutoconfStatus()));
  
  if (power_sensor_) {
    LOG_SENSOR("  ", "Power sensor", power_sensor_);
  }
  if (outdoor_temperature_sensor_) {
    LOG_SENSOR("  ", "Outdoor temperature sensor", outdoor_temperature_sensor_);
  }
  if (indoor_humidity_sensor_) {
    LOG_SENSOR("  ", "Indoor humidity sensor", indoor_humidity_sensor_);
  }
}

// Configuration setters
void MideaClimate::set_supported_modes(const std::vector<climate::ClimateMode>& modes) {
  supported_modes_ = modes;
}

void MideaClimate::set_supported_fan_modes(const std::vector<climate::ClimateFanMode>& modes) {
  supported_fan_modes_ = modes;
}

void MideaClimate::set_supported_swing_modes(const std::vector<climate::ClimateSwingMode>& modes) {
  supported_swing_modes_ = modes;
}

void MideaClimate::set_supported_presets(const std::vector<climate::ClimatePreset>& presets) {
  supported_presets_ = presets;
}

// Custom mode setters
void MideaClimate::set_custom_fan_modes(const std::vector<std::string>& modes) {
  custom_fan_modes_.clear();
  for (const auto& mode : modes) {
    custom_fan_modes_.push_back(mode);
  }
  ESP_LOGCONFIG(TAG, "Custom fan modes: %d configured", custom_fan_modes_.size());
}

void MideaClimate::set_custom_presets(const std::vector<std::string>& presets) {
  custom_presets_.clear();
  for (const auto& preset : presets) {
    custom_presets_.push_back(preset);
  }
  ESP_LOGCONFIG(TAG, "Custom presets: %d configured", custom_presets_.size());
}

void MideaClimate::sendUpdate() {
  // This is called when actual state changes occur
  ESP_LOGD(TAG, "State change detected - syncing to ESPHome/Home Assistant");
  ESP_LOGD(TAG, "Midea values: Indoor=%.1f°C, Target=%.1f°C, Mode=%d, Fan=%d, Swing=%d",
           this->getIndoorTemp(), this->getTargetTemp(), 
           static_cast<int>(this->getMode()), static_cast<int>(this->getFanMode()),
           static_cast<int>(this->getSwingMode()));
  
  // Store previous ESPHome state for comparison
  auto prev_esphome_mode = this->mode;
  auto prev_esphome_target = this->target_temperature;
  auto prev_esphome_current = this->current_temperature;
  
  // Sync MideaUART_v2 state to ESPHome climate state
  update_esphome_state();
  
  // Only publish if ESPHome state actually changed
  bool esphome_changed = false;
  if (this->mode != prev_esphome_mode) {
    ESP_LOGD(TAG, "ESPHome mode changed: %d -> %d", static_cast<int>(prev_esphome_mode), static_cast<int>(this->mode));
    esphome_changed = true;
  }
  if (std::abs(this->target_temperature - prev_esphome_target) > 0.1f) {
    ESP_LOGD(TAG, "ESPHome target changed: %.1f -> %.1f", prev_esphome_target, this->target_temperature);
    esphome_changed = true;
  }
  if (std::abs(this->current_temperature - prev_esphome_current) > 0.1f) {
    ESP_LOGD(TAG, "ESPHome current changed: %.1f -> %.1f", prev_esphome_current, this->current_temperature);
    esphome_changed = true;
  }
  
  if (esphome_changed) {
    this->publish_state();
    ESP_LOGD(TAG, "ESPHome state published to Home Assistant");
  } else {
    ESP_LOGV(TAG, "ESPHome state unchanged - skipping publish_state()");
  }

  // Update auxiliary sensors (only when they have valid data)
  if (power_sensor_ && this->getPowerUsage() > 0) {
    power_sensor_->publish_state(this->getPowerUsage());
    ESP_LOGV(TAG, "Power sensor updated: %.1fW", this->getPowerUsage());
  }

  if (outdoor_temperature_sensor_ && !std::isnan(this->getOutdoorTemp())) {
    outdoor_temperature_sensor_->publish_state(this->getOutdoorTemp());
    ESP_LOGV(TAG, "Outdoor temperature updated: %.1f°C", this->getOutdoorTemp());
  }

  if (indoor_humidity_sensor_ && !std::isnan(this->getIndoorHum())) {
    indoor_humidity_sensor_->publish_state(this->getIndoorHum());
    ESP_LOGV(TAG, "Indoor humidity updated: %.1f%%", this->getIndoorHum());
  }
}

// Helper functions for enum conversions
climate::ClimateMode MideaClimate::midea_mode_to_esphome(esphome::midea::ac::Mode mode) const {
  switch (mode) {
    case esphome::midea::ac::Mode::MODE_OFF: return climate::CLIMATE_MODE_OFF;
    case esphome::midea::ac::Mode::MODE_HEAT_COOL: return climate::CLIMATE_MODE_HEAT_COOL;
    case esphome::midea::ac::Mode::MODE_COOL: return climate::CLIMATE_MODE_COOL;
    case esphome::midea::ac::Mode::MODE_DRY: return climate::CLIMATE_MODE_DRY;
    case esphome::midea::ac::Mode::MODE_HEAT: return climate::CLIMATE_MODE_HEAT;
    case esphome::midea::ac::Mode::MODE_FAN_ONLY: return climate::CLIMATE_MODE_FAN_ONLY;
    default: return climate::CLIMATE_MODE_OFF;
  }
}

esphome::midea::ac::Mode MideaClimate::esphome_mode_to_midea(climate::ClimateMode mode) const {
  switch (mode) {
    case climate::CLIMATE_MODE_OFF: return esphome::midea::ac::Mode::MODE_OFF;
    case climate::CLIMATE_MODE_HEAT_COOL: return esphome::midea::ac::Mode::MODE_HEAT_COOL;
    case climate::CLIMATE_MODE_COOL: return esphome::midea::ac::Mode::MODE_COOL;
    case climate::CLIMATE_MODE_DRY: return esphome::midea::ac::Mode::MODE_DRY;
    case climate::CLIMATE_MODE_HEAT: return esphome::midea::ac::Mode::MODE_HEAT;
    case climate::CLIMATE_MODE_FAN_ONLY: return esphome::midea::ac::Mode::MODE_FAN_ONLY;
    default: return esphome::midea::ac::Mode::MODE_OFF;
  }
}

climate::ClimateFanMode MideaClimate::midea_fan_to_esphome(esphome::midea::ac::FanMode fan) const {
  switch (fan) {
    case esphome::midea::ac::FanMode::FAN_AUTO: return climate::CLIMATE_FAN_AUTO;
    case esphome::midea::ac::FanMode::FAN_LOW: return climate::CLIMATE_FAN_LOW;
    case esphome::midea::ac::FanMode::FAN_MEDIUM: return climate::CLIMATE_FAN_MEDIUM;
    case esphome::midea::ac::FanMode::FAN_HIGH: return climate::CLIMATE_FAN_HIGH;
    case esphome::midea::ac::FanMode::FAN_SILENT: return climate::CLIMATE_FAN_QUIET;
    case esphome::midea::ac::FanMode::FAN_TURBO: return climate::CLIMATE_FAN_HIGH; // Map turbo to high
    default: return climate::CLIMATE_FAN_AUTO;
  }
}

esphome::midea::ac::FanMode MideaClimate::esphome_fan_to_midea(climate::ClimateFanMode fan) const {
  switch (fan) {
    case climate::CLIMATE_FAN_AUTO: return esphome::midea::ac::FanMode::FAN_AUTO;
    case climate::CLIMATE_FAN_LOW: return esphome::midea::ac::FanMode::FAN_LOW;
    case climate::CLIMATE_FAN_MEDIUM: return esphome::midea::ac::FanMode::FAN_MEDIUM;
    case climate::CLIMATE_FAN_HIGH: return esphome::midea::ac::FanMode::FAN_HIGH;
    case climate::CLIMATE_FAN_QUIET: return esphome::midea::ac::FanMode::FAN_SILENT;
    default: return esphome::midea::ac::FanMode::FAN_AUTO;
  }
}

climate::ClimateSwingMode MideaClimate::midea_swing_to_esphome(esphome::midea::ac::SwingMode swing) const {
  switch (swing) {
    case esphome::midea::ac::SwingMode::SWING_OFF: return climate::CLIMATE_SWING_OFF;
    case esphome::midea::ac::SwingMode::SWING_VERTICAL: return climate::CLIMATE_SWING_VERTICAL;
    case esphome::midea::ac::SwingMode::SWING_HORIZONTAL: return climate::CLIMATE_SWING_HORIZONTAL;
    case esphome::midea::ac::SwingMode::SWING_BOTH: return climate::CLIMATE_SWING_BOTH;
    default: return climate::CLIMATE_SWING_OFF;
  }
}

esphome::midea::ac::SwingMode MideaClimate::esphome_swing_to_midea(climate::ClimateSwingMode swing) const {
  switch (swing) {
    case climate::CLIMATE_SWING_OFF: return esphome::midea::ac::SwingMode::SWING_OFF;
    case climate::CLIMATE_SWING_VERTICAL: return esphome::midea::ac::SwingMode::SWING_VERTICAL;
    case climate::CLIMATE_SWING_HORIZONTAL: return esphome::midea::ac::SwingMode::SWING_HORIZONTAL;
    case climate::CLIMATE_SWING_BOTH: return esphome::midea::ac::SwingMode::SWING_BOTH;
    default: return esphome::midea::ac::SwingMode::SWING_OFF;
  }
}

climate::ClimatePreset MideaClimate::midea_preset_to_esphome(esphome::midea::ac::Preset preset) const {
  switch (preset) {
    case esphome::midea::ac::Preset::PRESET_NONE: return climate::CLIMATE_PRESET_NONE;
    case esphome::midea::ac::Preset::PRESET_ECO: return climate::CLIMATE_PRESET_ECO;
    case esphome::midea::ac::Preset::PRESET_BOOST: return climate::CLIMATE_PRESET_BOOST;
    case esphome::midea::ac::Preset::PRESET_SLEEP: return climate::CLIMATE_PRESET_SLEEP;
    case esphome::midea::ac::Preset::PRESET_AWAY: return climate::CLIMATE_PRESET_AWAY;
    default: return climate::CLIMATE_PRESET_NONE;
  }
}

esphome::midea::ac::Preset MideaClimate::esphome_preset_to_midea(climate::ClimatePreset preset) const {
  switch (preset) {
    case climate::CLIMATE_PRESET_NONE: return esphome::midea::ac::Preset::PRESET_NONE;
    case climate::CLIMATE_PRESET_ECO: return esphome::midea::ac::Preset::PRESET_ECO;
    case climate::CLIMATE_PRESET_BOOST: return esphome::midea::ac::Preset::PRESET_BOOST;
    case climate::CLIMATE_PRESET_SLEEP: return esphome::midea::ac::Preset::PRESET_SLEEP;
    case climate::CLIMATE_PRESET_AWAY: return esphome::midea::ac::Preset::PRESET_AWAY;
    default: return esphome::midea::ac::Preset::PRESET_NONE;
  }
}

void MideaClimate::update_esphome_state() {
  // Sync MideaUART_v2 state to ESPHome climate state
  this->mode = this->midea_mode_to_esphome(this->getMode());
  this->target_temperature = this->getTargetTemp();
  this->current_temperature = this->getIndoorTemp();
  this->fan_mode = this->midea_fan_to_esphome(this->getFanMode());
  this->swing_mode = this->midea_swing_to_esphome(this->getSwingMode());
  this->preset = this->midea_preset_to_esphome(this->getPreset());
}


// MideaUART_v2 virtual method overrides for debugging
void MideaClimate::setup_() {
  ESP_LOGD(TAG, "MideaClimate::setup_() called");
  // Call the parent AirConditioner setup
  AirConditioner::setup_();
  ESP_LOGD(TAG, "AirConditioner::setup_() completed");
}

void MideaClimate::loop_() {
  // Call the parent AirConditioner loop (which should be empty) on parent
  AirConditioner::loop_();
}

void MideaClimate::onIdle_() {
  // Call the parent AirConditioner onIdle which sends status queries
  AirConditioner::onIdle_();
  
  // Skip state change detection during initial setup
  if (!setup_complete_) {
    return;
  }
  
  // Skip state change detection for 2 seconds after immediate control updates
  // This prevents redundant updates right after user interactions
  if (last_control_time_ > 0 && (esphome::millis() - last_control_time_) < CONTROL_UPDATE_SKIP_MS) {
    return;
  }
  
  // Check if state has actually changed since last update
  static float last_target_temp = NAN;
  static float last_current_temp = NAN;
  static esphome::midea::ac::Mode last_mode = esphome::midea::ac::Mode::MODE_OFF;
  static esphome::midea::ac::FanMode last_fan = esphome::midea::ac::FanMode::FAN_AUTO;
  static esphome::midea::ac::SwingMode last_swing = esphome::midea::ac::SwingMode::SWING_OFF;
  static esphome::midea::ac::Preset last_preset = esphome::midea::ac::Preset::PRESET_NONE;
  
  float current_target = this->getTargetTemp();
  float current_indoor = this->getIndoorTemp();
  auto current_mode = this->getMode();
  auto current_fan = this->getFanMode();
  auto current_swing = this->getSwingMode();
  auto current_preset = this->getPreset();
  
  bool state_changed = false;
  
  // Handle temperature changes with proper NAN checking
  if (std::isnan(last_target_temp) || std::abs(current_target - last_target_temp) > 0.1f) {
    if (!std::isnan(last_target_temp)) {  // Don't log on first run
      ESP_LOGD(TAG, "Target temperature changed: %.1f -> %.1f", last_target_temp, current_target);
    }
    state_changed = true;
    last_target_temp = current_target;
  }
  if (std::isnan(last_current_temp) || std::abs(current_indoor - last_current_temp) > 0.1f) {
    if (!std::isnan(last_current_temp)) {  // Don't log on first run
      ESP_LOGD(TAG, "Indoor temperature changed: %.1f -> %.1f", last_current_temp, current_indoor);
    }
    state_changed = true;
    last_current_temp = current_indoor;
  }
  if (current_mode != last_mode) {
    ESP_LOGD(TAG, "Mode changed: %d -> %d", static_cast<int>(last_mode), static_cast<int>(current_mode));
    state_changed = true;
    last_mode = current_mode;
  }
  if (current_fan != last_fan) {
    ESP_LOGD(TAG, "Fan mode changed: %d -> %d", static_cast<int>(last_fan), static_cast<int>(current_fan));
    state_changed = true;
    last_fan = current_fan;
  }
  if (current_swing != last_swing) {
    ESP_LOGD(TAG, "Swing mode changed: %d -> %d", static_cast<int>(last_swing), static_cast<int>(current_swing));
    state_changed = true;
    last_swing = current_swing;
  }
  if (current_preset != last_preset) {
    ESP_LOGD(TAG, "Preset changed: %d -> %d", static_cast<int>(last_preset), static_cast<int>(current_preset));
    state_changed = true;
    last_preset = current_preset;
  }
  
  // Only send update when state actually changes
  if (state_changed) {
    ESP_LOGD(TAG, "State changed - sending update to ESPHome/Home Assistant");
    this->sendUpdate();
  } else {
    // Still log status occasionally for debugging, avoid spamming state changes
    static uint32_t last_debug = 0;
    if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE && esphome::millis() - last_debug > DEBUG_LOG_INTERVAL_MS) {
      ESP_LOGV(TAG, "onIdle_: No state changes - Target=%.1f, Indoor=%.1f, Mode=%d",
               current_target, current_indoor, static_cast<int>(current_mode));
      last_debug = esphome::millis();
    }
  }
}

}  // namespace midea_direct
}  // namespace esphome
