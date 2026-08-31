# Midea Direct
ESPHome component re-written for ESP IDF, converted from dudanov MideaUART to be used with ESP32 C6 MINI (WT0132C6-S5).

Control is possible with a custom dongle. You can make it yourself according to my repo: [HERE](https://github.com/SaschaKP/midea-dongle).

A far from complete list of supported brands:
1. [Midea](https://www.midea.com/)
2. [Electrolux](https://www.electrolux.ru/)
3. [Qlima](https://www.qlima.com/)
4. [Artel](https://www.artelgroup.com/)
5. [Carrier](https://www.carrier.com/)
6. [Comfee](http://www.comfee-russia.ru/)
7. [Inventor](https://www.inventorairconditioner.com/)
8. [Dimstal/Simando](https://www.simando24.de/)
9. [Beko](https://www.beko.com/)

## Using
It's simple. Just use esphome and create your yaml as this below:

## 🏨 B&B & Energy Saving Optimization (Anti-Tamper Control)

If you are deploying this dongle in a B&B, hotel, or rental property, guests often abuse climate controls (e.g., setting the AC to 17°C in summer or 30°C in winter). This behavior severely increases energy bills and stresses the compressor.

With the configuration below, you can enforce a strict **comfort lock (e.g., 24°C)**. 
* In **COOL** mode, guests cannot go below 24°C.
* In **HEAT** mode, guests cannot go above 24°C.
* In **AUTO** mode, it locks tightly at 24°C.

This solution works flawlessly for **both Home Assistant dashboard commands and the original physical IR remote**. It uses an isolated ESPHome script acting as a software *debounce filter* to handle UART serial transmission latency on single-degree variations, avoiding recursive command loops.

### Configuration Example

Add the `script` component at the root of your YAML, and update the `climate` block as follows in the example:

```yaml
substitutions:
  name: "clima-test"
  friendly_name: "Clima Test"
  idname: clima_test
  #with this and the below code, you can disallow changing temperatures above or below threshold using IR or HASSIO/MQTT
  min_temp: "17"  # Allowed visual minimum
  max_temp: "30"  # Allowed visual maximum
  med_temp: "24"  # The absolute lock threshold (e.g., 24.0 °C)


#Include the board used  
esphome:
  name: $name
  friendly_name: $friendly_name
  name_add_mac_suffix: False

esp32:
  board: esp32-c6-devkitm-1
  #copy partitions files available on repository https://github.com/luar123/zigbee_esphome
  partitions: common/esphome/partitions_zb.csv
  framework: 
    type: esp-idf

status_led:
  pin:
    number: GPIO8
    inverted: True
    ignore_strapping_warning: true

#include common Wifi-OTA-API, etc
preferences:
  flash_write_interval: 
    seconds: 60

# Enable logging
logger:
  baud_rate: 0
  level: INFO

# Enable Home Assistant API
api:
  encryption:
    key: !secret api
  reboot_timeout: 0s

ota:
  - platform: esphome
    password: !secret ota

zigbee:
  id: "zb"
  router: true
  power_supply: 1
  components: none
  on_join:
    then:
      - logger.log:
          format: "Joined network"
          level: INFO

wifi:
  #mesh networking
  enable_btm: true
  enable_rrm: true
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  reboot_timeout: 120min

#possibility to use zigbee with ESP32 C6, in this case only ZigBee Router, but you can configure climate appliances
external_components:
  - source: github://luar123/zigbee_esphome
    components: [zigbee]
    refresh: 0s
  - source: github://SaschaKP/esphome-midea-direct
    components: [midea_direct]
    refresh: 0s

binary_sensor:
  - platform: gpio
    name: ZigBee Btn
    pin:
      number: 9
      ignore_strapping_warning: true
      mode:
        input: true
        pullup: true
      inverted: True
    id: button_1
    on_press:
      then:
        - zigbee.report: zb
    on_click:
      min_length: 10s
      max_length: 30s
      then:
        - zigbee.reset: zb

sensor:
  - platform: internal_temperature
    name: "Internal Temperature"
    id: temp
    filters:
      - delta: 0.1

uart:
  id: uart_1
  baud_rate: 9600
  tx_pin: GPIO16
  rx_pin: GPIO17

climate:
  - platform: midea_direct
    # Trigger the anti-tamper script safely without passing abstract C++ objects
    on_state:
      - lambda: |-
          id(controlla_limiti_clima).execute();
    id: $idname   # Use a unique id
    name: $friendly_name         # Use a unique name
    beeper: True
    autoconf: False              # you can also enable autoconf for auto configuration of capabilities
    period: 4s
    timeout: 3s                  # Optional
    num_attempts: 1              # Optional
    visual:                      # Optional
      min_temperature: $min_temp °C     # min: 17
      max_temperature: $max_temp °C     # max: 30
      temperature_step: 1 °C     # min: 0.5
    supported_modes:             # Optional. All capabilities in this section may be detected by autoconf.
      - HEAT_COOL
      - COOL
      - HEAT
      - DRY
      - FAN_ONLY
    custom_fan_modes:
      - SILENT
      - TURBO
    supported_presets:            # Optional. All capabilities in this section may be detected by autoconf.
      - BOOST
      - SLEEP
    custom_presets:               # Optional. All capabilities in this section may be detected by autoconf.
      - FREEZE_PROTECTION
    supported_swing_modes:        # All capabilities in this section detected by autoconf.
      - VERTICAL

script:
  - id: controlla_limiti_clima
    mode: restart # Resets the timer on consecutive keypresses, processing only the final stable value
    then:
      - delay: 3000ms # Essential buffer to let the UART internal variables sync with the AC motherboard
      - lambda: |-
          auto c = id($idname);
          const float med = ${med_temp};
          bool correggi = false;

          if (c->mode == CLIMATE_MODE_COOL) {
            if (c->target_temperature < med) {
              correggi = true;
            }
          }
          else if (c->mode == CLIMATE_MODE_HEAT) {
            if (c->target_temperature > med) {
              correggi = true;
            }
          }
          else if (c->mode == CLIMATE_MODE_HEAT_COOL) {
            if (c->target_temperature != med) {
              correggi = true;
            }
          }

          if (correggi) {
            auto call = c->make_call();
            call.set_target_temperature(med);
            call.perform();
          }
```


## My thanks

to the following people for their contributions to reverse engineering the UART protocol and source code in the following repositories:

* [Sergey Dudanov](https://github.com/dudanov/MideaUART)
* [Mac Zhou](https://github.com/mac-zhou/midea-msmart)
* [Rene Klootwijk](https://github.com/reneklootwijk/midea-uart)
* [NeoAcheron](https://github.com/NeoAcheron/midea-ac-py)

### Your thanks

If this project was useful to you, you can [buy me](https://paypal.me/uosteamclone) a Cup of coffee :)
