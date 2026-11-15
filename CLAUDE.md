# M5Stack Dial – ESPHome + custom LVGL UI

## Goal
Firmware for an M5Stack Dial (ESP32-S3, 240x240 round GC9A01A display, rotary encoder, touch, button, buzzer, RTC, NFC) that:
1. Connects to Home Assistant through the **ESPHome native API** (entities, OTA, logs).
2. Shows a **custom LVGL UI** written in C++ inside a local ESPHome external component.
3. Is a **display first**, and a **controller later** (encoder/touch/button -> HA actions).

UI specification: **TBD**. Do not invent screens or entities. Ask the user, then record the decisions in the "UI spec" section below.

## Architecture
- ESPHome owns: hardware drivers, Wi-Fi, HA API, OTA, entities, and LVGL init (`lvgl:` block, display, input devices).
- `components/dial_ui/` (local external component) owns: all custom UI built with raw `lv_*` calls on ESPHome's LVGL instance.
- HA -> UI: `platform: homeassistant` sensors/text_sensors whose `on_value` calls a method on the component (e.g. `id(ui).set_temperature(x)`).
- UI -> HA: the component publishes to an ESPHome entity (template number/sensor/event); YAML triggers on it and calls `homeassistant.action`. Do not touch the API protocol internals.
- Fallback if this hits limits: pure LVGL YAML (`lvgl:` pages/widgets), or fully custom PlatformIO firmware (user decision, not a default).

## Repo layout (target)
```
dial.yaml              # main config: hardware + packages
secrets.yaml           # NEVER commit; wifi_ssid, wifi_password, api_key, ota_password, ap_password
packages/              # optional: hardware.yaml, ui.yaml, ha_entities.yaml
components/dial_ui/    # __init__.py, dial_ui.h, dial_ui.cpp
```

## Hardware reference (M5Stack Dial v1.1, from M5Stack / devices.esphome.io)
- Board: ESP32-S3 (StampS3), `esp32: variant: esp32s3`, framework `esp-idf`, `platformio_options: board_build.flash_mode: dio`.
- I2C: SDA GPIO11, SCL GPIO12. RTC PCF8563 @0x51, touch FT5x06 @0x38 (interrupt pin unsupported, leave unset), NFC RC522 @0x28.
- SPI: MOSI GPIO5, CLK GPIO6. Display GC9A01A: CS GPIO7, DC GPIO4, RESET GPIO8, `invert_colors: true`. Backlight LEDC GPIO9 (exposed as light `display_backlight`).
- Encoder: A GPIO40, B GPIO41 (INPUT_PULLUP). Front button GPIO42 (inverted, pullup). Buzzer LEDC GPIO3. HOLD GPIO46 (drive high to keep power on battery).
- Ports: A = GPIO13/15, B = GPIO1/2.
- Flashing: first flash over USB-C (data cable). If the port does not show, enter download mode: hold G0 on the StampS3 (back), plug in USB, release. Reset after flashing. Later updates over Wi-Fi OTA.

## Known state
- Base config compiles and runs; the HA sensor -> display path works; backlight toggles from HA, button and touch.
- Display platform in use: `ili9xxx` model GC9A01A. M5Stack's own example uses `mipi_spi`; switch if the ESPHome version requires it.
- With LVGL: display needs `update_interval: never`, `auto_clear_enabled: false`, and no display lambda.

## Conventions and gotchas
- **LVGL version:** current ESPHome docs say ESPHome supports **LVGL 9**, so write C++ against the v9 API (e.g. `lv_screen_active()`, `lv_obj_remove_flag()`, `lv_image_*`), not v8 names (`lv_scr_act`, `lv_obj_clear_flag`, `lv_img_*`). Confirm with the installed ESPHome version and its bundled LVGL headers before writing C++, and use the LVGL docs version that matches.
- Do not use the removed/deprecated `custom_component` mechanism. Use a proper external component (Python + C++).
- ESPHome may compile LVGL with only the widgets/features used in YAML. If a widget used from C++ fails to link, declare one in YAML (verify against the ESPHome LVGL docs for the installed version).
- Call `lv_*` only from component `setup()`/`loop()` or YAML lambdas (same loop as the LVGL handler).
- LVGL's own printf may lack float support: format with `snprintf` into a buffer.
- Keep UI content inside the circle (240x240 round, corners clipped).
- `homeassistant.action` from the device needs "Allow the device to perform Home Assistant actions" enabled in the device's ESPHome integration options in HA.
- Secrets always via `!secret`; never hardcode passwords or keys; never commit `secrets.yaml`.

## Workflow
- Validate: `esphome config dial.yaml`
- Build: `esphome compile dial.yaml`
- Flash + logs: `esphome run dial.yaml` (USB first time, OTA afterwards); logs only: `esphome logs dial.yaml`
- After each change: validate -> compile -> flash -> check logs on the device. Report compile errors verbatim and fix root causes; do not guess pins or APIs, check the docs or the installed ESPHome source.
- Work in small steps (one screen or widget at a time) and keep the config compiling between steps.

## UI spec (to be defined)
- Pages/screens: TBD
- HA entities to display: TBD
- Inputs (encoder / touch / button) and the HA actions they trigger: TBD
- Look and feel (colors, fonts, icons, animations): TBD
- Screen wake/sleep behavior: currently backlight toggles on button; touch/knob wake; optional auto-off script (30 s).

## Open questions
- Exact ESPHome version and bundled LVGL version.
- Final display platform (`ili9xxx` vs `mipi_spi`).
- Whether NFC and the RTC are needed.

## References

### ESPHome (user docs)
- Getting started with ESPHome and Home Assistant: https://esphome.io/guides/getting_started_hassio.html
- LVGL component (config, pages, actions; states the supported LVGL version): https://esphome.io/components/lvgl/
- LVGL widgets in YAML: https://esphome.io/components/lvgl/widgets.html
- External components (local path source): https://esphome.io/components/external_components/
- Native API (`homeassistant.action`, `homeassistant.event`, enabling device actions in HA): https://www.esphome.io/components/api
- Component index (display, touchscreen, rotary_encoder, light, etc.): https://esphome.io/components/

### ESPHome (developer docs and C++ API)
- Developer docs home: https://developers.esphome.io/
- Component architecture (`__init__.py`, CONFIG_SCHEMA, `to_code`, directory layout): https://developers.esphome.io/architecture/components/
- Architectural overview: https://developers.esphome.io/architecture/overview/
- Why `custom_component` was removed (use external components instead): https://developers.esphome.io/blog/2025/02/19/about-the-removal-of-support-for-custom-components/
- C++ API, LVGL namespace: https://api-docs.esphome.io/namespaceesphome_1_1lvgl
- C++ API, `LvglComponent`: https://api-docs.esphome.io/classesphome_1_1lvgl_1_1_lvgl_component
- C++ API, `lvgl_esphome.h` source: https://api-docs.esphome.io/lvgl__esphome_8h_source

### LVGL (upstream docs; pick the version matching ESPHome's bundled LVGL)
- Latest: https://docs.lvgl.io/master
- Example, arc widget (v9.2): https://docs.lvgl.io/9.2/widgets/arc.html
- Example, arc widget (v8, for reference when reading older ESPHome/LVGL code): https://docs.lvgl.io/8/widgets/core/arc.html

### Home Assistant
- ESPHome integration (options incl. "Allow the device to perform Home Assistant actions"): https://home-assistant.io/integrations/esphome

### M5Stack Dial
- Product page (specs, download mode, HOLD pin): https://docs.m5stack.com/en/products/sku/K130
- Home Assistant integration guide: https://docs.m5stack.com/en/homeassistant/kit/dial
- Arduino compile and flash (download mode procedure): https://docs.m5stack.com/en/arduino/m5dial/program
- ESPHome device database entry (pin map, example config): https://devices.esphome.io/devices/m5stack-dial/
- M5Stack example YAML (`mipi_spi` display variant): https://github.com/m5stack/esphome-yaml/blob/main/examples/kit/dial-example.yaml
- Community UI example (ESPHome + LVGL controller for the Dial): https://community.home-assistant.io/t/m5stack-dial-home-assistant-controller-esphome/1019987

### Notes on the references
- Prefer the docs of the ESPHome version actually installed. The site documents the latest release, and the C++ API docs are versioned.
- When an API or config key is uncertain, check the ESPHome source (`esphome/components/lvgl/`) or the API docs above instead of guessing.
