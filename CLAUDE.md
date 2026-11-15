# M5Stack Dial – ESPHome + custom LVGL UI

## Goal
Firmware for an M5Stack Dial (ESP32-S3, 240x240 round GC9A01A display, rotary encoder with push button, touch, buzzer, RTC, NFC) that:
1. Connects to Home Assistant through the **ESPHome native API** (entities, OTA, logs).
2. Shows a **custom LVGL UI** written in C++ inside a local ESPHome external component.
3. Is a **display first**, and a **controller later** (encoder/touch -> HA actions).

The UI look, components and input vocabulary are defined by the design system in `design/` (see "Design system" below). The screen graph, navigation table and HA entities are **TBD**: do not invent screens or entities. Ask the user, then record the decisions in the "UI spec" section below.

## Architecture
- ESPHome owns: hardware drivers, Wi-Fi, HA API, OTA, entities, and LVGL init (`lvgl:` block with display and input devices only; no pages, widgets or fonts in YAML). ESPHome is only a data and command channel for the UI.
- `components/dial_ui/` (local external component) owns: the **entire** UI (screens, navigation, styles, fonts, icons) built with raw `lv_*` calls on ESPHome's LVGL instance. Fonts are compiled into the component (e.g. `lv_font_conv` C files), never declared in ESPHome YAML.
- HA -> UI: `platform: homeassistant` sensors/text_sensors whose `on_value` calls a generic method on the component (e.g. `id(ui).set_reading(room, metric, x)`), not one method per metric.
- UI -> HA: the component publishes to an ESPHome entity (template number/sensor/event); YAML triggers on it and calls `homeassistant.action`. Do not touch the API protocol internals.
- UI -> device: local commands (e.g. screen brightness) call ESPHome entities such as the `display_backlight` light from the component or YAML.
- Input: the encoder, its push button and touch are registered as LVGL input devices by ESPHome. The component only reads LVGL events (see "Input" in `design/README.md`); there is no second physical button.
- Fallback if this hits limits: pure LVGL YAML (`lvgl:` pages/widgets), or fully custom PlatformIO firmware (user decision, not a default).

## Repo layout (target)
```
homedial.yaml          # main config: hardware + packages (name is `homedial`)
secrets.yaml           # NEVER commit; wifi_ssid, wifi_password, homedial__encryption_key, homedial__ap_password
packages/              # optional: hardware.yaml, ui.yaml, ha_entities.yaml
components/dial_ui/    # __init__.py, dial_ui.h, dial_ui.cpp (+ generated font sources)
design/                # Dialed design system (tokens, LVGL notes, component specs)
```

## Hardware reference (M5Stack Dial v1.1, from M5Stack / devices.esphome.io)
- Board: ESP32-S3 (StampS3), `esp32: variant: esp32s3`, framework `esp-idf`, `platformio_options: board_build.flash_mode: dio`.
- I2C: SDA GPIO11, SCL GPIO12. RTC PCF8563 @0x51, touch FT5x06 @0x38 (interrupt pin unsupported, leave unset), NFC RC522 @0x28.
- SPI: MOSI GPIO5, CLK GPIO6. Display GC9A01A: CS GPIO7, DC GPIO4, RESET GPIO8, `invert_colors: true`. Backlight LEDC GPIO9 (exposed as light `display_backlight`).
- Encoder: A GPIO40, B GPIO41 (INPUT_PULLUP). Encoder push button GPIO42 (inverted, pullup): the **only** physical button. Buzzer LEDC GPIO3. HOLD GPIO46 (output, drive high to keep power on battery; currently misdeclared as an input binary sensor in `homedial.yaml`).
- Ports: A = GPIO13/15, B = GPIO1/2.
- Flashing: first flash over USB-C (data cable). If the port does not show, enter download mode: hold G0 on the StampS3 (back), plug in USB, release. Reset after flashing. Later updates over Wi-Fi OTA.

## Known state
- Per the user, an earlier base config compiled and ran, with the HA sensor -> display path working and the backlight toggling from HA, button and touch. The current `homedial.yaml` only partly matches that: it has no touchscreen, the encoder is a plain `rotary_encoder` sensor (not an LVGL input), the button toggles the backlight (to be replaced by the design's input model), and it still contains a demo `lvgl:` page (temp arc/label) and a placeholder entity `sensor.livingroom_temperature`, all to be removed once `dial_ui` renders the UI. Nothing has been re-verified in this repo yet.
- Display platform in use: `ili9xxx` model GC9A01A. M5Stack's own example uses `mipi_spi`; switch if the ESPHome version requires it.
- With LVGL: display needs `update_interval: never`, `auto_clear_enabled: false`, and no display lambda.

## Conventions and gotchas
- **LVGL version:** current ESPHome docs say ESPHome supports **LVGL 9**, so write C++ against the v9 API (e.g. `lv_screen_active()`, `lv_obj_remove_flag()`, `lv_image_*`), not v8 names (`lv_scr_act`, `lv_obj_clear_flag`, `lv_img_*`). Confirm with the installed ESPHome version and its bundled LVGL headers before writing C++, and use the LVGL docs version that matches.
- Do not use the removed/deprecated `custom_component` mechanism. Use a proper external component (Python + C++).
- Include the design tokens from `design/lvgl/ui_tokens.h` in the component. An external component cannot reach `../design` by default, so decide on a copy/symlink/build step (open question) and keep `design/` the single source of truth.
- Stock Montserrat sizes depend on the `lv_conf.h` that ESPHome generates, which we do not control. The design needs 12, 14, 16, 20, 28, 40, 48; the plan is custom fonts built into the component, so verify what ESPHome's LVGL build provides before relying on any stock font.
- ESPHome may compile LVGL with only the widgets/features used in YAML. If a widget used from C++ fails to link, declare one in YAML (verify against the ESPHome LVGL docs for the installed version).
- Call `lv_*` only from component `setup()`/`loop()` or YAML lambdas (same loop as the LVGL handler).
- LVGL's own printf may lack float support: format with `snprintf` into a buffer.
- Keep UI content inside the circle (240x240 round, corners clipped).
- `homeassistant.action` from the device needs "Allow the device to perform Home Assistant actions" enabled in the device's ESPHome integration options in HA.
- Secrets always via `!secret`; never hardcode passwords or keys; never commit `secrets.yaml`.

## Workflow
- Validate: `esphome config homedial.yaml`
- Build: `esphome compile homedial.yaml`
- Flash + logs: `esphome run homedial.yaml` (USB first time, OTA afterwards); logs only: `esphome logs homedial.yaml`
- After each change: validate -> compile -> flash -> check logs on the device. Report compile errors verbatim and fix root causes; do not guess pins or APIs, check the docs or the installed ESPHome source.
- Work in small steps (one screen or widget at a time) and keep the config compiling between steps.

## UI spec (to be defined)
- Pages/screens: TBD (pick from the design's screen components)
- Navigation table (which page opens which; one table shared by touch and encoder): TBD
- HA entities to display (rooms, metrics, limits, power, energy, weather): TBD
- Settings and HA actions triggered from the UI: TBD
- Language: TBD (English fits stock ASCII fonts; French needs Latin-1 custom fonts)
- Theme: TBD (dark only or dark + day)
- Inputs: defined by the design system (`design/README.md` "Input"): encoder turn / press / long press (600 ms = back, or settings at top level), touch tap / swipe / hold. One physical button only.
- Look and feel: defined by the design system (tokens, Montserrat sizes, flat style, motion at or under 250 ms).
- Screen wake/sleep: per the design, dim to 30% after 30 s without input, wake on any input or on an ALERT. This replaces the earlier "button toggles backlight" and "auto-off" behavior.

## Open questions
- Exact ESPHome version and bundled LVGL version.
- Final display platform (`ili9xxx` vs `mipi_spi`).
- Whether NFC and the RTC are needed.
- How `dial_ui` gets `design/lvgl/ui_tokens.h` and generated fonts into the build.
- Whether the encoder push short/long press is handled by the LVGL encoder indev (`LV_EVENT_CLICKED` / `LV_EVENT_LONG_PRESSED`) or by a `binary_sensor` `on_click` calling the component.

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
- The screen graph and navigation are defined by this project (UI spec above), not by the design system.
- The design assumes a single physical button (encoder push); a second button is optional and never required.
