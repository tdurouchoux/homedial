# M5Stack Dial – ESPHome + custom LVGL UI

## Goal
Firmware for an M5Stack Dial (ESP32-S3, 240x240 round GC9A01A display, rotary encoder with push button, touch, buzzer, RTC, NFC) that:
1. Connects to Home Assistant through the **ESPHome native API** (entities, OTA, logs).
2. Shows a **custom LVGL UI** written in C++ inside a local ESPHome external component.
3. Is a **display first**, and a **controller later** (encoder/touch -> HA actions).

The UI look, components and input vocabulary are defined by the design system in `design/` (see "Design system" below). The screen graph, navigation and HA entities are defined in `spec.md`. Do not invent screens or entities: ask the user, then record the decisions there.

## Architecture
- ESPHome owns: hardware drivers, Wi-Fi, HA API, OTA, entities, and LVGL init (`lvgl:` block with display and input devices only; no pages, widgets or fonts in YAML). ESPHome is only a data and command channel for the UI.
- `components/dial_ui/` (local external component) owns: the **entire** UI (screens, navigation, styles, icons) built with raw `lv_*` calls on ESPHome's LVGL instance. Text uses the stock Montserrat sizes enabled by `esphome: build_flags` in `homedial.yaml` (English UI); weather icons are A8 images generated from the design SVGs by `tools/gen_weather_icons.js` (re-run it after changing `design/components/WeatherIcon`); room icons are not drawn yet. No fonts in the `lvgl:` YAML block.
- HA -> UI: `platform: homeassistant` sensors/text_sensors are referenced by id in the `dial_ui:` config (rooms -> metrics -> `sensor: <id>`); the component subscribes to them itself. `id(ui).set_reading(...)` from an `on_value` lambda is only an escape hatch. Details in `contract.md`.
- UI -> HA: the component publishes to an ESPHome entity (template number/sensor/event); YAML triggers on it and calls `homeassistant.action`. Do not touch the API protocol internals.
- UI -> device: local commands (e.g. screen brightness) call ESPHome entities such as the `display_backlight` light from the component or YAML.
- Input: the encoder (with its push button as enter key, `long_press_time: 600ms`) and the FT5x06 touch are registered as LVGL input devices in `packages/hardware.yaml`. The component only reads LVGL events (`CLICKED`, `LONG_PRESSED`, `KEY`, `GESTURE`; see "Input" in `design/README.md`); there is no second physical button.
- Fallback if this hits limits: pure LVGL YAML (`lvgl:` pages/widgets), or fully custom PlatformIO firmware (user decision, not a default).

## Repo layout (target)
```
homedial.yaml          # main config: hardware + packages (name is `homedial`)
secrets.yaml           # NEVER commit; wifi_ssid, wifi_password, homedial__encryption_key, homedial__ap_password
packages/              # optional: hardware.yaml, ui.yaml, ha_entities.yaml
components/dial_ui/    # __init__.py, dial_ui.h, dial_ui.cpp (+ generated font sources)
plan.md                # implementation milestones
spec.md                # product spec: screens, navigation, settings, behaviour
contract.md            # data contract between ESPHome YAML and dial_ui (draft)
design/                # Dialed design system (tokens, LVGL notes, component specs)
```

## Hardware reference (M5Stack Dial v1.1, from M5Stack / devices.esphome.io)
- Board: ESP32-S3FN8 (StampS3, 8 MB flash, no PSRAM), `esp32: variant: esp32s3`, `flash_mode: dio`, `flash_size: 8MB`, framework `esp-idf`.
- I2C: SDA GPIO11, SCL GPIO12. RTC PCF8563 @0x51, touch FT5x06 @0x38 (interrupt pin unsupported, leave unset), NFC RC522 @0x28.
- SPI: MOSI GPIO5, CLK GPIO6. Display GC9A01A: CS GPIO7, DC GPIO4, RESET GPIO8, `invert_colors: true`. Backlight LEDC GPIO9 (exposed as light `display_backlight`).
- Encoder: A GPIO40, B GPIO41 (INPUT_PULLUP). Encoder push button GPIO42 (inverted, pullup): the **only** physical button. Buzzer LEDC GPIO3. HOLD GPIO46 (output, drive high to keep power on battery; a `gpio` switch `power_hold` with `restore_mode: ALWAYS_ON`). GPIO3 and GPIO46 are strapping pins: the warning is silenced with `ignore_strapping_warning`.
- Ports: A = GPIO13/15, B = GPIO1/2.
- Flashing: first flash over USB-C (data cable). If the port does not show, enter download mode: hold G0 on the StampS3 (back), plug in USB, release. Reset after flashing. Later updates over Wi-Fi OTA.

## Known state
- Implementation follows the milestones in `plan.md`. M1 (hardware packages, LVGL input devices, HOLD, empty screen) works on the device (user, 2026-11-15). M2 (rooms model, Room and Sensor pages) works on the device (user). M3 (navigation, light motion, Settings entry) works on the device (user). M4 (History: 24 h hourly chart with scrub) runs on the device (full check needs hours of uptime). M5 (Weather now, hourly forecast) works on the device (user). M6 (Electricity) works on the device (user). M7 (Settings: luminosity, buzzer volume, screen off, power off) compiles; not yet confirmed on the device.
- Host unit tests for the LVGL-free `model_*` files: `tests/host/run.sh`.
- Flash: the StampS3 is an ESP32-S3FN8 (8 MB flash, no PSRAM, per M5Stack's Dial docs); `esp32: flash_size: 8MB` gives two 3.75 MB OTA app slots. M7 uses 36.9 % of one. Check the size line of each compile.
- The M5Stack Dial has a touchscreen (FT5x06 on I2C); it is part of the hardware and the UI input model.
- Display platform in use: `ili9xxx` model GC9A01A. M5Stack's own example uses `mipi_spi`; switch if the ESPHome version requires it.
- With LVGL: display needs `update_interval: never`, `auto_clear_enabled: false`, and no display lambda.

## Conventions and gotchas
- **LVGL version:** ESPHome 2026.10.2 bundles **LVGL 9.5.0** (`LVGL_VERSION` in `esphome/components/lvgl/__init__.py`), so write C++ against the v9 API (e.g. `lv_screen_active()`, `lv_obj_remove_flag()`, `lv_image_*`), not v8 names (`lv_scr_act`, `lv_obj_clear_flag`, `lv_img_*`). Confirm with the installed ESPHome version and its bundled LVGL headers before writing C++, and use the LVGL docs version that matches.
- Do not use the removed/deprecated `custom_component` mechanism. Use a proper external component (Python + C++).
- Design tokens: `components/dial_ui/__init__.py` copies `design/lvgl/ui_tokens.h` into the build's `src/` at codegen, so C++ does `#include "ui_tokens.h"` and `design/` stays the single source of truth. Do not use an `-I` build flag: ESP-IDF builds forward only `-D`/`-W` flags (`esphome/build_gen/espidf.py`).
- ESPHome compiles only the files directly in `components/dial_ui/`, not subfolders (`esphome/loader.py: resources`). Keep the component flat and group files by prefix (`model_*.h`, `widget_*.cpp`, `screen_*.cpp`).
- ESPHome generates `lv_conf.h` and sets every `LV_USE_*` not used in YAML to 0, but honours `esphome: build_flags` (`generate_lv_conf_h` in `esphome/components/lvgl/__init__.py`). Enable C++-only widgets (`LV_USE_ARC`, `LV_USE_CHART`, ...) and the Montserrat sizes 12, 14, 16, 20, 28, 40, 48 there.
- Call `lv_*` only from component `setup()`/`loop()` or YAML lambdas (same loop as the LVGL handler).
- LVGL's own printf may lack float support: format with `snprintf` into a buffer.
- Keep UI content inside the circle (240x240 round, corners clipped).
- `homeassistant.action` from the device needs "Allow the device to perform Home Assistant actions" enabled in the device's ESPHome integration options in HA.
- Secrets always via `!secret`; never hardcode passwords or keys; never commit `secrets.yaml`.

## Workflow
- Validate: `esphome config homedial.yaml`. Claude has no `esphome` install, so it runs `uvx esphome config homedial.yaml` instead. This needs a throwaway `secrets.yaml` next to the config (gitignored; never commit it).
- Compile: Claude can run `uvx esphome compile homedial.yaml` in a cloud session on the `homedial` environment (allows `dl.espressif.com`, `components.espressif.com`, `components-file.espressif.com` and GitHub). A clean compile is not an on-device check.
- Building and flashing the hardware is done by the user, who reports compile errors and device logs verbatim. Claude never claims a build, flash or on-device check passed unless the user confirmed it.
- After each change: validate, then hand over to the user to build and flash. Fix root causes of reported errors; do not guess pins or APIs, check the docs or the installed ESPHome source.
- Work in small steps (one screen or widget at a time) and keep the config compiling between steps.

## UI spec
Product decisions (screens, navigation, settings, behaviour) live in `spec.md`, not here. The look and input vocabulary live in `design/`. Do not invent screens or entities; ask the user and record decisions in `spec.md`.

## Open questions
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

## Design system (Dialed)
The UI design system lives in `design/`. Read it before writing any UI code.
- `design/README.md`: principles, metric types, input/gesture vocabulary, iconography.
- `design/tokens.json` + `design/lvgl/ui_tokens.h`: single source of truth for colours, spacing, sizes. Never hardcode values; use the `DIALED_*` macros.
- `design/lvgl.md`: LVGL mapping (fonts, arcs, charts, gestures, encoder groups).
- `design/components/<Name>/README.md` + `preview.html`: one component each; build components first, then screens.
- The screen graph and navigation are defined by this project in `spec.md`, not by the design system. Where `spec.md` differs from `design/`, `spec.md` wins until the design is updated.
- The design assumes a single physical button (encoder push); a second button is optional and never required.
