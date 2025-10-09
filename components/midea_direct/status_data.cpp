#include "status_data.h"

namespace esphome {
namespace midea {
namespace ac {

float StatusData::getTargetTemp() const {
  uint8_t tmp = this->getValue_(2, 15) + 16;
  uint8_t tmpNew = this->getValue_(13, 31);
  if (tmpNew)
    tmp = tmpNew + 12;
  float temp = static_cast<float>(tmp);
  if (this->getValue_(2, 16))
    temp += 0.5F;
  return temp;
}

void StatusData::setTargetTemp(float temp) {
  uint8_t tmp = static_cast<uint8_t>(temp * 4.0F) + 1;
  uint8_t integer = tmp / 4;
  this->setValue_(18, integer - 12, 31);
  integer -= 16;
  if (integer < 1 || integer > 14)
    integer = 1;
  this->setValue_(2, ((tmp & 2) << 3) | integer, 31);
}

static float getTemp(int integer, int decimal, bool fahrenheits) {
  integer -= 50;
  if (!fahrenheits && decimal > 0)
    return static_cast<float>(integer / 2) + static_cast<float>(decimal) * ((integer >= 0) ? 0.1F : -0.1F);
  if (decimal >= 5)
    return static_cast<float>(integer / 2) + ((integer >= 0) ? 0.5F : -0.5F);
  return static_cast<float>(integer) * 0.5F;
}
float StatusData::getIndoorTemp() const { return getTemp(this->getValue_(11), this->getValue_(15, 15), this->isFahrenheits()); }
float StatusData::getOutdoorTemp() const { return getTemp(this->getValue_(12), this->getValue_(15, 15, 4), this->isFahrenheits()); }
float StatusData::getHumiditySetpoint() const { return static_cast<float>(this->getValue_(19, 127)); }

Mode StatusData::getMode() const { return this->getPower_() ? this->getRawMode() : Mode::MODE_OFF; }

void StatusData::setMode(Mode mode) {
  if (mode != Mode::MODE_OFF) {
    this->setPower_(true);
    this->setValue_(2, mode, 7, 5);
  } else {
    this->setPower_(false);
  }
}

FanMode StatusData::getFanMode() const {
  //some ACs return 30 for LOW and 50 for MEDIUM. Note though, in appMode, this device still uses 40/60
  uint8_t fanMode = this->getValue_(3);
  if (fanMode == 30) {
    fanMode = FAN_LOW;
  } else if (fanMode == 50) {
    fanMode = FAN_MEDIUM;
  }
  return static_cast<FanMode>(fanMode); 
}

Preset StatusData::getPreset() const {
  if (this->getEco_())
    return Preset::PRESET_ECO;
  if (this->getTurbo_())
    return Preset::PRESET_BOOST;
  if (this->getSleep_())
    return Preset::PRESET_SLEEP;
  if (this->getFreezeProtection_())
    return Preset::PRESET_AWAY;
  return Preset::PRESET_NONE;
}

void StatusData::setPreset(Preset preset) {
  this->setEco_(false);
  this->setSleep_(false);
  this->setTurbo_(false);
  this->setFreezeProtection_(false);
  switch (preset) {
    case Preset::PRESET_NONE:
      break;
    case Preset::PRESET_ECO:
      this->setEco_(true);
      break;
    case Preset::PRESET_BOOST:
      this->setTurbo_(true);
      break;
    case Preset::PRESET_SLEEP:
      this->setSleep_(true);
      break;
    case Preset::PRESET_AWAY:
      this->setFreezeProtection_(true);
      break;
    default:
      break;
  }
}

static uint8_t bcd2u8(uint8_t bcd) { return 10 * (bcd >> 4) + (bcd & 15); }

float StatusData::getPowerUsage() const {
  uint32_t power = 0;
  const uint8_t *ptr = this->data_.data() + 18;
  for (uint32_t weight = 1;; weight *= 100, --ptr) {
    power += weight * bcd2u8(*ptr);
    if (weight == 10000)
      return static_cast<float>(power) * 0.1F;
  }
}

}  // namespace ac
}  // namespace midea
}  // namespace esphome
