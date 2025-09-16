# vivid controller

## prerequisites
- arduino-cli or Arduino IDE

## setup

### install heltec boards
```bash
arduino-cli config add board_manager.additional_urls https://resource.heltec.cn/download/package_heltec_esp32_index.json
arduino-cli core update-index
arduino-cli core install Heltec-esp32:esp32
```

### install libs

```bash
arduino-cli lib install "Heltec ESP32 Dev-Boards"
arduino-cli lib install "Adafruit PWM Servo Driver Library"
```

## notes on structure

* **node\_config.h**
  each board must have its own `node_config.h`.
  create it by copying and editing the provided template:

  ```bash
  cp node_config.h.template node_config.h
  ```

* **animations**
  both files need to be modified to add an animation (or change its params):

  * `anim_schema.h`
  * `animations.h`

## build and upload

arduino-cli usage:

```bash
arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V3 .
arduino-cli upload -p /dev/ttyUSB0 --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V3 .
```

## access the web ui

- ssid: LeaderNode-AP
- password: leds1234
- ip: 192.168.4.1
