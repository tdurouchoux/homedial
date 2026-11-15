# Implementation plan: homedial firmware (ESPHome + dial_ui)

Status: M1–M5 confirmed on the device (M4 history needs hours of uptime for a full check). M6 (Electricity) written, unit-tested and compiled (flash 36.8 % of 3.75 MB, RAM 32 %); awaiting on-device check.

## Context
`spec.md` (screens, navigation, behaviour), `design/` (Dialed look) and `contract.md` (ESPHome ↔ UI data contract) are now settled enough to build. `homedial.yaml` is still the prototype: a YAML demo arc page, no touchscreen, no encoder as LVGL input, no HOLD, no `dial_ui`. Goal: build the real firmware in small flashable steps (vertical slice first), validating every step with `uvx esphome config` and handing builds/flashes to the user.

Decisions taken for this plan: English UI, dark theme only, placeholder HA entity ids (user fills them in), vertical-slice milestones. Toolchain verified in source: ESPHome 2026.10.2, LVGL 9.5.0.

## Target layout
```
homedial.yaml                 # esphome/esp32/wifi/api/ota + packages + external_components
packages/hardware.yaml        # i2c, spi, display, backlight, buzzer, HOLD, encoder, button, touch, lvgl indevs, time
packages/ha_entities.yaml     # homeassistant sensors/text_sensors (placeholder entity ids)
packages/ui.yaml              # dial_ui: block (rooms, thresholds, contract limit, triggers)
components/dial_ui/           # flat: ESPHome compiles only files directly in this folder
  __init__.py                 # schema + to_code
  dial_ui.h/.cpp              # Component: setup/loop, owns model + screens + nav
  model_*.h/.cpp              # pure C++, no LVGL: readings, status, history ring, rain parser, condition map
  widget_*.h/.cpp             # one per design component (ArcGauge, StatusLabel, LocationLabel, PageDots, TrendChart, ListMenu, WeatherIcon...)
  screen_*.h/.cpp             # room, sensor, history, weather, forecast, power, settings, adjust
  font_*.c                    # generated icon fonts (+ tools/ script outside the component)
tests/host/                   # g++ unit tests for model/
```

## Cross-cutting choices
- **Tokens:** `__init__.py` copies `design/lvgl/ui_tokens.h` into the build `src/` with `copy_file_if_changed` (`-I` flags are dropped by the ESP-IDF build generator).
- **LVGL features:** `generate_lv_conf_h` (`components/lvgl/__init__.py`) zeroes unused `LV_USE_*` but honours `esphome: build_flags`. Add `-DLV_USE_CHART=1`, `-DLV_USE_ARC=1`, `-DLV_USE_LABEL=1` etc. for widgets used only from C++, and `-DLV_FONT_MONTSERRAT_{12,14,16,20,28,40,48}=1`.
- **Fonts:** English → stock Montserrat sizes via the build flags above (they include `°` and `LV_SYMBOL_*`). This replaces the "custom text fonts" plan in CLAUDE.md for now; update CLAUDE.md. Custom fonts only for the weather icon font (48/32 px) and room icon font (16 px), generated with `lv_font_conv` (npx) and committed as `.c`.
- **LVGL access:** `dial_ui` takes `lvgl_id: cv.use_id(LvglComponent)`; setup priority after `setup_priority::PROCESSOR`; screens created with `lv_obj_create(nullptr)` and loaded with `lv_screen_load_anim`. Encoder group: use the default group ESPHome creates (`lv_group_get_default()`).
- **Model is LVGL-free** (`model_*` files) so it can be unit-tested on the host with g++.
- **Input:** `lvgl: encoders:` (rotary_encoder + GPIO42 button, `long_press_time: 600ms`) and `touchscreens:` (ft5x06 @0x38). Component reads `LV_EVENT_KEY`/`CLICKED`/`LONG_PRESSED`/`GESTURE` only; all routed to `nav_next/prev/open/back` (design `lvgl.md`). Resolves the CLAUDE.md open question in favour of the LVGL indev.
- **HOLD:** a `gpio` switch with `restore_mode: ALWAYS_ON` (set at hardware setup), so Power off uses `switch::Switch`.
- **Display:** keep `ili9xxx` GC9A01A (still supported in 2026.10.2); switch to `mipi_spi` only if compile/runtime requires.

## Milestones (each ends with `uvx esphome config`, commit, user build + flash)

**M1 – Hardware + empty UI.** Split `homedial.yaml` into packages; add HOLD (GPIO46 `gpio` switch, `ALWAYS_ON`), ft5x06 touch, button GPIO42 (inverted, pullup), encoder as LVGL indev, buzzer, `time: homeassistant` (timezone comes from HA 2026.3+). Remove the demo page and placeholder sensor. `components/dial_ui` skeleton: schema with `lvgl_id`, C++ draws a `bg` screen with the bezel arc and "homedial" label. `lvgl: widgets: []` suppresses ESPHome's hello_world page.
*User check:* boots, screen shows label, logs show encoder/touch events.

**M2 – Data model + Room and Sensor pages (one room).** Schema: `rooms[] {id, name, icon, metrics[] {metric, sensor: use_id, history}}`, `thresholds` per metric (+ per-room override), `contract_limit_w`. C++: subscribe via `add_on_state_callback`, status from thresholds, NaN → `--`/OFFLINE, API-connected polling (`is_connected_with_state_subscription()`) → stale. Widgets: ArcGauge, StatusLabel, LocationLabel, PageDots. Screens: ScreenRoom (1/2/3 layouts; metrics are temperature, humidity, air quality), ScreenSensor. Redraw dirty widgets ≤ 1 Hz.
*Host tests:* status thresholds, NaN, staleness.
*Done:* `model_metric.h`, `model_room.h`, `widget_*`, `screen_room.*`, `screen_sensor.*`; navigation subset ahead of M3 so the pages can be reached: turn = sibling room / metric, press on Room = first metric, tap on a reading = that metric, long press on Sensor = back. The encoder reaches pages through one focus object per screen in edit mode (`LV_EVENT_KEY`, `SHORT_CLICKED`, `LONG_PRESSED`; `CLICKED` also fires after a long press, so it is not used).

**M3 – Navigation.** Page table `{kind, room, metric}`; carousel (rooms → weather → power, no wrap); deeper/back; Room tap on a reading; 200 ms slide; Settings entry (long press, swipe down, 600 ms touch hold) as a stub screen.
*Done:* `model_nav.h` (pure `nav_apply(page, action)`, host-tested) is the single table for encoder, swipes and taps. Carousel is rooms only until Weather (M5) and Power (M6) exist. Two Sensor screens alternate (kept from the slide version). Page slides were removed after on-device testing (laggy); pages switch instantly. Settings placeholder `screen_settings.h`. Swipe handling calls `lv_indev_wait_release` so a swipe's release is not also a tap. Touch hold uses its own 600 ms timer (cancelled on release, gesture or > 12 px movement), independent of LVGL's touch long-press time, which M4 sets to 250 ms for the chart scrub.

**M4 – History.** Ring buffer (24 closed hourly buckets + open; running mean; once-a-minute in-memory sample; missing hours; wall clock from HA time). TrendChart (`lv_chart`, LINE, cursor) + ScreenHistory with encoder and touch scrub (250 ms hold).
*Host tests:* rollover, gaps, minute sampling, clock-not-valid.
*Done:* `model_history.h` (25 points: 24 closed hours + current), `widget_trend_chart.*` (ext y array, cursor, last-point and scrub dots, dashed limit `lv_line`), `screen_history.*`, History in `model_nav.h` (only for `history: true` readings; level 2: turn/swipe never change page, swipe down / long press back). Touch scrub uses its own 250 ms hold timer on the chart (the touch indev's long-press time stays at its default, so slow taps on Room readings still work); the chart stops bubbling gestures while scrubbing. `time_id` is required when any metric has `history: true`. Build flags add `LV_USE_CHART` and `LV_USE_LINE`.

**M5 – Weather now + forecast.** Weather sensors (temperature attribute, condition text_sensor, next-rain attributes `1_hour_forecast` / `forecast_time_ref`). Rain parser (Python-repr dict, `Temps sec` = dry) → segmented 60-min arc. Condition → icon table. Icon font. `on_forecast_request` trigger (automation in schema) + `load_forecast(JsonArrayConst)`; requested on connect, every 30 min, on page open if > 30 min old. ScreenWeather, ScreenForecast (snap strip, encoder focus).
*Host tests:* rain parser (dry, rain, partial, malformed), forecast row filtering, icon map.
*User check in HA:* run the `response_template` in Developer tools first.
*Done:* `model_weather.h` (condition map, ISO time, rain slot parser, runs, rain line, forecast rows; host-tested), weather and room carousel in `model_nav.h`, `widget_weather.*` (A8 icon images, segmented RainArc), `screen_weather.*`, `screen_forecast.*`, `on_forecast_request` trigger + `load_forecast(JsonVariantConst)`. Icons: `tools/gen_weather_icons.js` renders the design SVGs with Chromium into `weather_icons.c` (A8 images tinted `ink`, not an icon font: the stroked SVGs would need outline tooling). The `response_template` was checked with plain Jinja2 against a response shaped like HA's `weather/services.py` (not in a real HA).

**M6 – Power.** `power: {sensor: use_id}`; ScreenPower with 80/100 % colours and kW switch above 9999 W.
*Done:* `model_power.h` (host-tested), `screen_power.*`, Electricity as the last carousel page in `model_nav.h`, `power: {sensor, contract_limit}` schema, carousel limit of 7 pages validated, ring sweep on arrival, Settings reachable from every carousel page.

**M7 – Settings.** ListMenu, ScreenAdjust; luminosity (`backlight: use_id(light.LightState)`), buzzer volume (`buzzer: use_id(output.FloatOutput)`), both persisted via `global_preferences`; Screen off; Power off (`power_hold: use_id(switch_.Switch)`); Exit.

**M8 – Dim and sleep.** 30 s → 30 %, 60 s → off, wake on any input, first input after screen-off swallowed; alerts don't wake.

## Docs to update along the way
- `CLAUDE.md`: fonts (stock sizes via build flags), token include method, input via LVGL indev, resolved open questions.
- `contract.md` / `spec.md`: anything changed while implementing; record the rain-line wording once chosen.

## Open items that do not block M1–M4
Rain-line wording; weather city on the Forecast title; icon SVG sources for the weather/room icon fonts (design only has previews); RTC as early time source; real entity ids.

## Verification
- Every step: `uvx esphome config homedial.yaml` with a throwaway `secrets.yaml` (not committed).
- `uvx esphome compile homedial.yaml` in the container (cloud environment `homedial`, which allows the Espressif and GitHub download hosts). M1 compiled cleanly on 2026-11-15: no `dial_ui` warnings, RAM 30.9 %, flash 61.0 % of the app partition.
- `tests/host`: `g++ -std=c++17` unit tests for the `model_*` files, run in the container.
- User builds/flashes each milestone and reports logs; nothing is claimed working on the device without their confirmation.
