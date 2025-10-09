#include "capabilities.h"
#include "frame_data.h"
#include "esphome/core/log.h"

namespace esphome {
namespace midea {
namespace ac {

static const char *TAG = "Capabilities";

enum CapabilityID : uint16_t {
  CAPABILITY_INDOOR_HUMIDITY = 0x0015,
  CAPABILITY_SILKY_COOL = 0x0018,
  CAPABILITY_SMART_EYE = 0x0030,
  CAPABILITY_WIND_ON_ME = 0x0032,
  CAPABILITY_WIND_OF_ME = 0x0033,
  CAPABILITY_ACTIVE_CLEAN = 0x0039,
  CAPABILITY_ONE_KEY_NO_WIND_ON_ME = 0x0042,
  CAPABILITY_BREEZE_CONTROL = 0x0043,
  CAPABILITY_FAN_SPEED_CONTROL = 0x0210,
  CAPABILITY_PRESET_ECO = 0x0212,
  CAPABILITY_PRESET_FREEZE_PROTECTION = 0x0213,
  CAPABILITY_MODES = 0x0214,
  CAPABILITY_SWING_MODES = 0x0215,
  CAPABILITY_POWER = 0x0216,
  CAPABILITY_NEST = 0x0217,
  CAPABILITY_AUX_ELECTRIC_HEATING = 0x0219,
  CAPABILITY_PRESET_TURBO = 0x021A,
  CAPABILITY_HUMIDITY = 0x021F,
  CAPABILITY_UNIT_CHANGEABLE = 0x0222,
  CAPABILITY_LIGHT_CONTROL = 0x0224,
  CAPABILITY_TEMPERATURES = 0x0225,
  CAPABILITY_BUZZER = 0x022C,
};

static uint16_t read_u16(const uint8_t *data) { return (data[1] << 8) | data[0]; }

class CapabilityData {
 public:
  CapabilityData(const FrameData &data) :
    it_(data.data() + 2),
    end_(data.data() + data.size() - 1),
    num_(*(data.data() + 1)) {}
  // Get capability ID
  CapabilityID id() const { return static_cast<CapabilityID>(read_u16(this->it_)); }
  // Read-only indexed access to capability data
  const uint8_t &operator[](uint8_t idx) const { return *(this->it_ + idx + 3); }
  // Get size of capability data
  uint8_t size() const { return this->it_[2]; }
  // 
  bool isValid() const { return num_ && this->available_() >= 3; }
  // One more request needed
  bool isNeedMore() const { return this->available_() == 2 && *this->it_ != 0; }
  // Advance to next capability
  void advance() {
    this->it_ += this->size() + 3;
    --this->num_;
  }
 private:
  uint8_t available_() const { return std::distance(this->it_, this->end_); }
  // Iterator
  const uint8_t *it_;
  // End of data
  const uint8_t *const end_;
  // Number of capabilities in answer
  uint8_t num_;
};

bool Capabilities::read(const FrameData &frame) {
  if (frame.size() < 14)
    return false;

  CapabilityData cap(frame);

  for (; cap.isValid(); cap.advance()) {
    if (!cap.size())
      continue;
    const uint8_t uval = cap[0];
    const bool bval = uval;
    switch (cap.id()) {
      case CAPABILITY_INDOOR_HUMIDITY:
        this->indoorHumidity_ = bval;
        break;
      case CAPABILITY_SILKY_COOL:
        this->silkyCool_ = bval;
        break;
      case CAPABILITY_SMART_EYE:
        this->smartEye_ = uval == 1;
        break;
      case CAPABILITY_WIND_ON_ME:
        this->windOnMe_ = uval == 1;
        break;
      case CAPABILITY_WIND_OF_ME:
        this->windOfMe_ = uval == 1;
        break;
      case CAPABILITY_ACTIVE_CLEAN:
        this->activeClean_ = uval == 1;
        break;
      case CAPABILITY_ONE_KEY_NO_WIND_ON_ME:
        this->oneKeyNoWindOnMe_ = uval == 1;
        break;
      case CAPABILITY_BREEZE_CONTROL:
        this->breezeControl_ = uval == 1;
        break;
      case CAPABILITY_FAN_SPEED_CONTROL:
        this->fanSpeedControl_ = uval != 1;
        break;
      case CAPABILITY_PRESET_ECO:
        this->ecoMode_ = uval == 1;
        this->specialEco_ = uval == 2;
        break;
      case CAPABILITY_PRESET_FREEZE_PROTECTION:
        this->frostProtectionMode_ = uval == 1;
        break;
      case CAPABILITY_MODES:
        switch (uval) {
          case 0:
            this->heatMode_ = false;
            this->coolMode_ = true;
            this->dryMode_ = true;
            this->autoMode_ = true;
            break;
          case 1:
            this->coolMode_ = true;
            this->heatMode_= true;
            this->dryMode_ = true;
            this->autoMode_ = true;
            break;
          case 2:
            this->coolMode_ = false;
            this->dryMode_ = false;
            this->heatMode_ = true;
            this->autoMode_ = true;
            break;
          case 3:
            this->coolMode_ = true;
            this->dryMode_ = false;
            this->heatMode_ = false;
            this->autoMode_ = false;
            break;
        }
        break;
      case CAPABILITY_SWING_MODES:
        switch (uval) {
          case 0:
            this->leftrightFan_ = false;
            this->updownFan_ = true;
            break;
          case 1:
            this->leftrightFan_ = true;
            this->updownFan_ = true;
            break;
          case 2:
            this->leftrightFan_ = false;
            this->updownFan_ = false;
            break;
          case 3:
            this->leftrightFan_ = true;
            this->updownFan_ = false;
            break;
        }
        break;
      case CAPABILITY_POWER:
        switch (uval) {
          case 0:
          case 1:
            this->powerCal_ = false;
            this->powerCalSetting_ = false;
            break;
          case 2:
            this->powerCal_ = true;
            this->powerCalSetting_ = false;
            break;
          case 3:
            this->powerCal_ = true;
            this->powerCalSetting_ = true;
            break;
        }
        break;
      case CAPABILITY_NEST:
        switch (uval) {
          case 0:
            this->nestCheck_ = false;
            this->nestNeedChange_ = false;
            break;
          case 1:
          case 2:
            this->nestCheck_ = true;
            this->nestNeedChange_ = false;
            break;
          case 3:
            this->nestCheck_ = false;
            this->nestNeedChange_ = true;
            break;
          case 4:
            this->nestCheck_ = true;
            this->nestNeedChange_ = true;
            break;
        }
        break;
      case CAPABILITY_AUX_ELECTRIC_HEATING:
        this->electricAuxHeating_ = bval;
        break;
      case CAPABILITY_PRESET_TURBO:
        switch (uval) {
          case 0:
            this->turboHeat_ = false;
            this->turboCool_ = true;
            break;
          case 1:
            this->turboHeat_ = true;
            this->turboCool_ = true;
            break;
          case 2:
            this->turboHeat_ = false;
            this->turboCool_ = false;
            break;
           case 3:
            this->turboHeat_ = true;
            this->turboCool_ = false;
            break;
        }
        break;
      case CAPABILITY_HUMIDITY:
        switch (uval) {
          case 0:
            this->autoSetHumidity_ = false;
            this->manualSetHumidity_ = false;
            break;
          case 1:
            this->autoSetHumidity_ = true;
            this->manualSetHumidity_ = false;
            break;
          case 2:
            this->autoSetHumidity_ = true;
            this->manualSetHumidity_ = true;
            break;
          case 3:
            this->autoSetHumidity_ = false;
            this->manualSetHumidity_ = true;
            break;
        }
        break;
      case CAPABILITY_UNIT_CHANGEABLE:
        this->unitChangeable_ = !bval;
        break;
      case CAPABILITY_LIGHT_CONTROL:
        this->lightControl_ = bval;
        break;
      case CAPABILITY_TEMPERATURES:
        if (cap.size() >= 6) {
          this->minTempCool_ = static_cast<float>(uval) * 0.5f;
          this->maxTempCool_ = static_cast<float>(cap[1]) * 0.5f;
          this->minTempAuto_ = static_cast<float>(cap[2]) * 0.5f;
          this->maxTempAuto_ = static_cast<float>(cap[3]) * 0.5f;
          this->minTempHeat_ = static_cast<float>(cap[4]) * 0.5f;
          this->maxTempHeat_ = static_cast<float>(cap[5]) * 0.5f;
          this->decimals_ = (cap.size() > 6) ? cap[6] : cap[2];
        }
        break;
      case CAPABILITY_BUZZER:
        this->buzzer_ = bval;
        break;
    }
  }

  if (cap.isNeedMore())
    return true;

  return false;
}

#define LOG_CAPABILITY(str, condition) \
  if (condition) \
    ESP_LOGCONFIG(TAG, str);

void Capabilities::dump() const {
  ESP_LOGCONFIG(TAG, "CAPABILITIES REPORT:");
  if (this->autoMode_) {
    ESP_LOGCONFIG(TAG, "  [x] AUTO MODE");
    ESP_LOGCONFIG(TAG, "      - MIN TEMP: %.1f", this->minTempAuto_);
    ESP_LOGCONFIG(TAG, "      - MAX TEMP: %.1f", this->maxTempAuto_);
  }
  if (this->coolMode_) {
    ESP_LOGCONFIG(TAG, "  [x] COOL MODE");
    ESP_LOGCONFIG(TAG, "      - MIN TEMP: %.1f", this->minTempCool_);
    ESP_LOGCONFIG(TAG, "      - MAX TEMP: %.1f", this->maxTempCool_);
  }
  if (this->heatMode_) {
    ESP_LOGCONFIG(TAG, "  [x] HEAT MODE");
    ESP_LOGCONFIG(TAG, "      - MIN TEMP: %.1f", this->minTempHeat_);
    ESP_LOGCONFIG(TAG, "      - MAX TEMP: %.1f", this->maxTempHeat_);
  }
  LOG_CAPABILITY("  [x] DRY MODE", this->dryMode_);
  LOG_CAPABILITY("  [x] ECO MODE", this->ecoMode_);
  LOG_CAPABILITY("  [x] SPECIAL ECO", this->specialEco_);
  LOG_CAPABILITY("  [x] FROST PROTECTION MODE", this->frostProtectionMode_);
  LOG_CAPABILITY("  [x] TURBO COOL", this->turboCool_);
  LOG_CAPABILITY("  [x] TURBO HEAT", this->turboHeat_);
  LOG_CAPABILITY("  [x] FANSPEED CONTROL", this->fanSpeedControl_);
  LOG_CAPABILITY("  [x] BREEZE CONTROL", this->breezeControl_);
  LOG_CAPABILITY("  [x] LIGHT CONTROL", this->lightControl_);
  LOG_CAPABILITY("  [x] UPDOWN FAN", this->updownFan_);
  LOG_CAPABILITY("  [x] LEFTRIGHT FAN", this->leftrightFan_);
  LOG_CAPABILITY("  [x] AUTO SET HUMIDITY", this->autoSetHumidity_);
  LOG_CAPABILITY("  [x] MANUAL SET HUMIDITY", this->manualSetHumidity_);
  LOG_CAPABILITY("  [x] INDOOR HUMIDITY", this->indoorHumidity_);
  LOG_CAPABILITY("  [x] POWER CAL", this->powerCal_);
  LOG_CAPABILITY("  [x] POWER CAL SETTING", this->powerCalSetting_);
  LOG_CAPABILITY("  [x] BUZZER", this->buzzer_);
  LOG_CAPABILITY("  [x] ACTIVE CLEAN", this->activeClean_);
  LOG_CAPABILITY("  [x] DECIMALS", this->decimals_);
  LOG_CAPABILITY("  [x] ELECTRIC AUX HEATING", this->electricAuxHeating_);
  LOG_CAPABILITY("  [x] NEST CHECK", this->nestCheck_);
  LOG_CAPABILITY("  [x] NEST NEED CHANGE", this->nestNeedChange_);
  LOG_CAPABILITY("  [x] ONE KEY NO WIND ON ME", this->oneKeyNoWindOnMe_);
  LOG_CAPABILITY("  [x] SILKY COOL", this->silkyCool_);
  LOG_CAPABILITY("  [x] SMART EYE", this->smartEye_);
  LOG_CAPABILITY("  [x] UNIT CHANGEABLE", this->unitChangeable_);
  LOG_CAPABILITY("  [x] WIND OF ME", this->windOfMe_);
  LOG_CAPABILITY("  [x] WIND ON ME", this->windOnMe_);
}

}  // namespace ac
}  // namespace midea
}  // namespace esphome
