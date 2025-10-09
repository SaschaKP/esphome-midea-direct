#pragma once
#include <set>

namespace esphome {
namespace midea {

class FrameData;

namespace ac {

class Capabilities {
 public:
  // Read from frames
  bool read(const FrameData &data);
  // Dump capabilities
  void dump() const;

  // Control humidity
  bool autoSetHumidity() const { return this->autoSetHumidity_; };
  bool activeClean() const { return this->activeClean_; };
  bool breezeControl() const { return this->breezeControl_; };
  bool buzzer() const { return this->buzzer_; }
  bool decimals() const { return this->decimals_; }
  bool electricAuxHeating() const { return this->electricAuxHeating_; }
  bool fanSpeedControl() const { return this->fanSpeedControl_; }
  bool indoorHumidity() const { return this->indoorHumidity_; }
  // Control humidity
  bool manualSetHumidity() const { return this->manualSetHumidity_; }
  bool nestCheck() const { return this->nestCheck_; }
  bool nestNeedChange() const { return this->nestNeedChange_; }
  bool oneKeyNoWindOnMe() const { return this->oneKeyNoWindOnMe_; }
  bool powerCal() const { return this->powerCal_; }
  bool powerCalSetting() const { return this->powerCalSetting_; }
  bool silkyCool() const { return this->silkyCool_; }
  // Intelligent eye function
  bool smartEye() const { return this->smartEye_; }
  // Temperature unit can be changed between Celsius and Fahrenheit
  bool unitChangeable() const { return this->unitChangeable_; }
  bool windOfMe() const { return this->windOfMe_; }
  bool windOnMe() const { return this->windOnMe_; }
  
  /* MODES */

  bool supportAutoMode() const { return this->autoMode_; }
  bool supportCoolMode() const { return this->coolMode_; }
  bool supportHeatMode() const { return this->heatMode_; }
  bool supportDryMode() const { return this->dryMode_; }

  /* PRESETS */

  bool supportFrostProtectionPreset() const { return this->frostProtectionMode_; }
  bool supportTurboPreset() const { return this->turboCool_ || this->turboHeat_; }
  bool supportEcoPreset() const { return this->ecoMode_ || this->specialEco_; }

  /* SWING MODES */

  bool supportVerticalSwing() const { return this->updownFan_; }
  bool supportHorizontalSwing() const { return this->leftrightFan_; }
  bool supportBothSwing() const { return this->updownFan_ && this->leftrightFan_; }

  /* TEMPERATURES */

  float maxTempAuto() const { return this->maxTempAuto_; }
  float maxTempCool() const { return this->maxTempCool_; }
  float maxTempHeat() const { return this->maxTempHeat_; }
  float minTempAuto() const { return this->minTempAuto_; }
  float minTempCool() const { return this->minTempCool_; }
  float minTempHeat() const { return this->minTempHeat_; }

  // Ability to turn LED display off
  bool supportLightControl() const { return this->lightControl_; }

 protected:
  bool updownFan_{false};
  bool leftrightFan_{false};
  bool autoMode_{false};
  bool coolMode_{false};
  bool dryMode_{false};
  bool ecoMode_{false};
  bool specialEco_{false};
  bool frostProtectionMode_{false};
  bool heatMode_{false};
  bool turboCool_{false};
  bool turboHeat_{false};
  bool autoSetHumidity_{false};
  bool activeClean_{false};
  bool breezeControl_{false};
  bool buzzer_{false};
  bool decimals_{false};
  bool electricAuxHeating_{false};
  bool fanSpeedControl_{true};
  bool indoorHumidity_{false};
  bool lightControl_{false};
  bool manualSetHumidity_{false};
  float maxTempAuto_{30};
  float maxTempCool_{30};
  float maxTempHeat_{30};
  float minTempAuto_{17};
  float minTempCool_{17};
  float minTempHeat_{17};
  bool nestCheck_{false};
  bool nestNeedChange_{false};
  bool oneKeyNoWindOnMe_{false};
  bool powerCal_{false};
  bool powerCalSetting_{false};
  bool silkyCool_{false};
  bool smartEye_{false};
  bool unitChangeable_{false};
  bool windOfMe_{false};
  bool windOnMe_{false};
};

}  // namespace ac
}  // namespace midea
}  // namespace esphome
