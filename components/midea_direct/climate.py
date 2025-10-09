import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, sensor, uart
from esphome.components.climate import ClimateMode, ClimateFanMode, ClimateSwingMode, ClimatePreset
from esphome.const import (
    CONF_ID,
    CONF_PERIOD,
    CONF_TIMEOUT,
    CONF_SUPPORTED_MODES,
    CONF_SUPPORTED_FAN_MODES, 
    CONF_SUPPORTED_SWING_MODES,
    CONF_SUPPORTED_PRESETS,
    CONF_BEEPER,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    ICON_POWER,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_WATT,
    UNIT_CELSIUS,
)

DEPENDENCIES = ["climate", "uart"]
AUTO_LOAD = ["sensor"]
CODEOWNERS = ["@your-github-username"]

CONF_NUM_ATTEMPTS = "num_attempts"
CONF_AUTOCONF = "autoconf"
CONF_POWER_USAGE = "power_usage"
CONF_OUTDOOR_TEMPERATURE = "outdoor_temperature"
CONF_INDOOR_HUMIDITY = "indoor_humidity"
CONF_CUSTOM_FAN_MODES = "custom_fan_modes"
CONF_CUSTOM_PRESETS = "custom_presets"

midea_ns = cg.esphome_ns.namespace("midea_direct")
MideaClimate = midea_ns.class_("MideaClimate", climate.Climate, cg.Component, uart.UARTDevice)

SUPPORTED_CLIMATE_MODES = {
    "HEAT_COOL": ClimateMode.CLIMATE_MODE_HEAT_COOL,
    "COOL": ClimateMode.CLIMATE_MODE_COOL,
    "HEAT": ClimateMode.CLIMATE_MODE_HEAT,
    "DRY": ClimateMode.CLIMATE_MODE_DRY,
    "FAN_ONLY": ClimateMode.CLIMATE_MODE_FAN_ONLY,
}

SUPPORTED_FAN_MODES = {
    "AUTO": ClimateFanMode.CLIMATE_FAN_AUTO,
    "LOW": ClimateFanMode.CLIMATE_FAN_LOW,
    "MEDIUM": ClimateFanMode.CLIMATE_FAN_MEDIUM,
    "HIGH": ClimateFanMode.CLIMATE_FAN_HIGH,
    "QUIET": ClimateFanMode.CLIMATE_FAN_QUIET,
}

SUPPORTED_SWING_MODES = {
    "OFF": ClimateSwingMode.CLIMATE_SWING_OFF,
    "VERTICAL": ClimateSwingMode.CLIMATE_SWING_VERTICAL,
    "HORIZONTAL": ClimateSwingMode.CLIMATE_SWING_HORIZONTAL,
    "BOTH": ClimateSwingMode.CLIMATE_SWING_BOTH,
}

SUPPORTED_PRESETS = {
    "NONE": ClimatePreset.CLIMATE_PRESET_NONE,
    "ECO": ClimatePreset.CLIMATE_PRESET_ECO,
    "BOOST": ClimatePreset.CLIMATE_PRESET_BOOST,
    "SLEEP": ClimatePreset.CLIMATE_PRESET_SLEEP,
}

SUPPORTED_CUSTOM_FAN_MODES = [
    "SILENT",    # FAN_SILENT = 20
    "TURBO",     # FAN_TURBO = 100
]

SUPPORTED_CUSTOM_PRESETS = [
    "FREEZE_PROTECTION",  # PRESET_FREEZE_PROTECTION
]

CONFIG_SCHEMA = climate.climate_schema(MideaClimate).extend({
    cv.GenerateID(): cv.declare_id(MideaClimate),
    
    # Core MideaUART_v2 settings
    cv.Optional(CONF_PERIOD, default="1s"): cv.time_period,
    cv.Optional(CONF_TIMEOUT, default="2s"): cv.time_period, 
    cv.Optional(CONF_NUM_ATTEMPTS, default=3): cv.int_range(min=1, max=5),
    cv.Optional(CONF_AUTOCONF, default=True): cv.boolean,
    cv.Optional(CONF_BEEPER, default=False): cv.boolean,
    
    # Mode support
    cv.Optional(CONF_SUPPORTED_MODES): cv.ensure_list(cv.enum(SUPPORTED_CLIMATE_MODES, upper=True)),
    cv.Optional(CONF_SUPPORTED_FAN_MODES): cv.ensure_list(cv.enum(SUPPORTED_FAN_MODES, upper=True)),
    cv.Optional(CONF_SUPPORTED_SWING_MODES): cv.ensure_list(cv.enum(SUPPORTED_SWING_MODES, upper=True)),
    cv.Optional(CONF_SUPPORTED_PRESETS): cv.ensure_list(cv.enum(SUPPORTED_PRESETS, upper=True)),
    
    cv.Optional(CONF_CUSTOM_FAN_MODES): cv.ensure_list(cv.one_of(*SUPPORTED_CUSTOM_FAN_MODES, upper=True)),
    cv.Optional(CONF_CUSTOM_PRESETS): cv.ensure_list(cv.one_of(*SUPPORTED_CUSTOM_PRESETS, upper=True)),
    
    cv.Optional(CONF_POWER_USAGE): sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        icon=ICON_POWER,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_OUTDOOR_TEMPERATURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        icon=ICON_THERMOMETER,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_INDOOR_HUMIDITY): sensor.sensor_schema(
        unit_of_measurement="%",
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)
    
    # Set timing parameters
    cg.add(var.set_period(config[CONF_PERIOD].total_milliseconds))
    cg.add(var.set_timeout(config[CONF_TIMEOUT].total_milliseconds))
    cg.add(var.set_num_attempts(config[CONF_NUM_ATTEMPTS]))
    cg.add(var.set_autoconf(config[CONF_AUTOCONF]))
    cg.add(var.set_beeper_config(config[CONF_BEEPER]))
    
    # Set supported modes if specified
    if CONF_SUPPORTED_MODES in config:
        cg.add(var.set_supported_modes(config[CONF_SUPPORTED_MODES]))
    if CONF_SUPPORTED_FAN_MODES in config:
        cg.add(var.set_supported_fan_modes(config[CONF_SUPPORTED_FAN_MODES]))
    if CONF_SUPPORTED_SWING_MODES in config:
        cg.add(var.set_supported_swing_modes(config[CONF_SUPPORTED_SWING_MODES]))
    if CONF_SUPPORTED_PRESETS in config:
        cg.add(var.set_supported_presets(config[CONF_SUPPORTED_PRESETS]))
    
    if CONF_CUSTOM_FAN_MODES in config:
        cg.add(var.set_custom_fan_modes(config[CONF_CUSTOM_FAN_MODES]))
    if CONF_CUSTOM_PRESETS in config:
        cg.add(var.set_custom_presets(config[CONF_CUSTOM_PRESETS]))
    
    # Set sensors
    if CONF_POWER_USAGE in config:
        sens = await sensor.new_sensor(config[CONF_POWER_USAGE])
        cg.add(var.set_power_sensor(sens))
    if CONF_OUTDOOR_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_TEMPERATURE])
        cg.add(var.set_outdoor_temperature_sensor(sens))
    if CONF_INDOOR_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_INDOOR_HUMIDITY])
        cg.add(var.set_indoor_humidity_sensor(sens))