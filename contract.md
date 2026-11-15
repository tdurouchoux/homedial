# ESPHome ↔ dial_ui contract (draft)

How data moves between the ESPHome-owned side (drivers, HA API, entities) and the `dial_ui` external component (model + LVGL view). Status: **draft**, nothing here is implemented yet. Decisions marked **[decide]** need the user.

Verified against **ESPHome 2026.10.2** (bundles **LVGL 9.5.0**, `esphome/components/lvgl/__init__.py: LVGL_VERSION`) and Home Assistant core `dev` sources (Nov 2026). Every ESPHome/HA feature used below is cited with the file it was checked in.

## 1. Principles

1. **Two layers, one seam.** YAML is the *adapter* (knows HA entity ids, hardware, actions). `dial_ui` is the *model + view* (knows rooms, metrics, pages, widgets). YAML never references a widget; C++ never contains an HA entity id or action name.
2. **Topology is static config, values are dynamic.** Rooms, their metrics, which series keep history, thresholds and limits are declared once in the `dial_ui:` YAML block and validated by `esphome config`. Pages are built in `setup()` from that config, before any data arrives. Data only updates labels and arcs.
3. **Inbound callbacks only write the model.** Sensor callbacks and action responses store values and set a dirty flag. `dial_ui::loop()` redraws dirty widgets at most once per second (design: "update readings at 1 Hz"). All of these run in the ESPHome main loop, the same loop as the LVGL handler, so no locking is needed.
4. **Missing is a value.** `NaN` means missing; the view shows `--` + OFFLINE. There is no "last known value" without a staleness rule (§4).

## 2. Identifiers

| Key | Values | Where defined |
| --- | --- | --- |
| `room` | user ids (`living`, `bedroom`, ...), order = carousel order | `dial_ui.rooms[]` |
| `metric` | `temperature`, `humidity`, `co2`, `pm25` (room metrics, fixed page order); `power`; `outdoor_temperature` | C++ enum, closed set |
| `series` | `(room, metric)` or a global metric (`power`) | implied by the config |

Room display name (≤ 12 chars) and optional icon id live in the config, so every screen reads them from one table (design `lvgl.md` "Rooms and sensors").

## 3. Inbound: HA → UI

### 3.1 Wiring (decided): `dial_ui` references sensors by id

```yaml
sensor:
  - platform: homeassistant
    id: living_temp
    entity_id: sensor.living_temperature

dial_ui:
  id: ui
  time_id: ha_time
  rooms:
    - id: living
      name: Living room
      icon: sofa                 # optional
      metrics:
        - metric: temperature
          sensor: living_temp    # cv.use_id(sensor.Sensor)
          history: true
        - metric: humidity
          sensor: living_hum
```

In `to_code`, the component gets each `sensor::Sensor*` and calls `add_on_state_callback` on it (exists: `sensor/sensor.h:121`, `text_sensor/text_sensor.h:59`). Benefits: ids are checked by `esphome config`, no string `room`/`metric` arguments in lambdas, and the component sees which series exist before boot.

Decided by the user (2026-11-15). The public setter below stays as an escape hatch for a value computed in YAML (`on_value: lambda: id(ui).set_reading(...)`), not as the normal path.

### 3.2 Public C++ surface (called by the component itself or YAML lambdas)

```cpp
// Live values. NaN = missing.
void set_reading(Room room, Metric metric, float value);
void set_power(float watts);
void set_weather_now(float temp, const std::string &ha_condition, int rain_minutes, WeatherWarning warning);
// Forecast payload from a homeassistant.action response (§6).
void load_forecast(JsonArrayConst rows);
void forecast_request_failed(const std::string &error);
```

`JsonArrayConst` is what `on_success` exposes: the action trigger passes `JsonObjectConst response` (`api/__init__.py`, `homeassistant_service_to_code`; `api/homeassistant_service.h:68`).

### 3.3 Value semantics (verified)

- `platform: homeassistant` sensor publishes `NAN` when the HA state does not parse as a number (`unavailable`, `unknown`): `homeassistant/sensor/homeassistant_sensor.cpp`.
- `attribute:` is supported on homeassistant sensors and text sensors (`homeassistant/__init__.py: HOME_ASSISTANT_IMPORT_SCHEMA`). HA sends attributes as `str(value)` (`esphome/manager.py: _send_home_assistant_state`), so scalars work (`temperature` of a `weather.*` entity) but **lists arrive as Python repr, not JSON**: never read forecasts or history through an attribute.
- HA only sends on change, and resends every subscribed state when the API reconnects.

### 3.4 Status (ok / warn / danger) is computed in the component

From thresholds in the config (per metric, overridable per room), and for power from `contract_limit_w` (80 % / 100 %, design rule). YAML never sends a state. Thresholds and `contract_limit_w` live in the `dial_ui:` YAML block (decided): per-metric defaults, overridable per room; changing them needs a reflash/OTA.

## 4. Connectivity and staleness

A value that does not change is never resent, so a timeout per sensor would wrongly mark stable sensors OFFLINE. Instead:

- **Source of truth:** `api::global_api_server->is_connected_with_state_subscription()` (`api/api_server.h:215`; same as the YAML condition `api.connected: {state_subscription_only: true}`). This is true only for a client that subscribed to HA states (i.e. Home Assistant), not for a logs client.
- The component polls it in `loop()`. On **true → false**: all HA-fed series are marked stale (OFFLINE, `--`). On **false → true**: values come back by themselves (HA resends) and the forecast is re-requested (§6).
- `NaN` from one sensor marks only that series OFFLINE.

## 5. History

Only series with `history: true` keep history (the spec's History pages: room metrics). Power has none for now (Energy page dropped).

Decided by the user (2026-11-15): **device-only, RAM only, hourly over 24 h.** No backfill from Home Assistant, nothing written to flash. After a reboot or OTA the chart starts empty and fills over 24 h.

### 5.1 Model

Per series, a fixed ring of **24 closed hourly buckets + 1 open bucket** (the current hour), each `{start_epoch, mean, count}`, aligned on wall-clock hours. 25 points ≤ the design's 60-point chart limit. RAM ≈ 25 × 12 B per series.

- Each live value (not `NaN`) updates the open bucket's running mean.
- On the hour rollover the open bucket closes and the ring shifts. An hour with no value (device off, HA disconnected, sensor `NaN` all hour) is stored as missing and drawn as `LV_CHART_POINT_NONE`, never interpolated.
- Wall clock: `time: - platform: homeassistant` (`homeassistant/time/`). Until the time is valid nothing is recorded. **[decide later]** the PCF8563 RTC could provide time before HA connects (CLAUDE.md open question on RTC); not needed for this contract.
- Because HA only sends on change, a stable sensor yields few callbacks. The component therefore also samples the current value of each history series once a minute into the open bucket, so a flat hour still gets a mean. This reads the value already in memory; it is not a request to HA.

### 5.2 Not used: backfill from HA

Checked and possible (`recorder.get_statistics` through `homeassistant.action` with `capture_response`), but not wanted. If it is ever added, it plugs in as a request trigger + `load_history()` without changing the model above.

## 6. Weather

| Data | Source | Transport |
| --- | --- | --- |
| Outdoor temperature | `weather.<x>` attribute `temperature` | homeassistant `sensor` with `attribute:` |
| Condition | `weather.<x>` state (`sunny`, `rainy`, ...) | homeassistant `text_sensor` |
| Rain remaining (min) | **[decide]** no standard HA entity; needs an HA template sensor or a provider entity | homeassistant `sensor` |
| Weather warning | **[decide]** provider-specific entity | homeassistant `text_sensor` |
| Hourly forecast (24 h) | `weather.get_forecasts` action, `type: hourly` | request/response, see below |

HA condition strings are mapped to the 6 design icons by one table in C++.

**Forecast request/response.** The component cannot call `homeassistant.action` itself without touching API internals, so it fires a trigger declared in its config, and YAML performs the action:

```yaml
dial_ui:
  on_forecast_request:
    - homeassistant.action:
        action: weather.get_forecasts
        data:
          entity_id: weather.home
          type: hourly
        capture_response: true
        response_template: >-
          [{%- for f in (response.values() | list | first).forecast[:24] -%}
          [{{ as_timestamp(f.datetime) | int }},"{{ f.condition }}",{{ f.temperature | round(1) }}]{{ ',' if not loop.last }}
          {%- endfor -%}]
        on_success:
          - lambda: id(ui).load_forecast(response["response"]);
        on_error:
          - lambda: id(ui).forecast_request_failed(error);
```

Checked against the sources:
- `capture_response`, `response_template`, `on_success` (with `response`), `on_error` (with `error`) exist; `capture_response` requires `on_success` (`api/__init__.py: _validate_response_config`).
- HA renders `response_template` with `response` = the action result and sends back `{"response": <rendered>}` as JSON (`esphome/manager.py: _handle_service_call_with_response`). Hence `response["response"]` in the lambda.
- One API message is ≤ 32 KiB on ESP32 (`api/api_frame_helper.h: MAX_MESSAGE_SIZE`); 24 compact rows ≈ 1 KB.
- Requires **"Allow the device to perform Home Assistant actions"** in the HA device options (`CONF_ALLOW_SERVICE_CALLS` in `manager.py`).
- **Not verified yet:** the `weather.get_forecasts` response shape (`{entity_id: {forecast: [...]}}`) was not checked in HA source this session, and the template has not been run. Check in HA Developer tools > Actions when implementing.

Requested on connect, every 30 min, and when the Forecast page opens with data older than 30 min. One request at a time.

## 7. Outbound: UI → device and UI → HA

### 7.1 Local (no HA involved)

| Command | Target | Contract |
| --- | --- | --- |
| Luminosity, dim 30 %, screen off/wake | `light` `display_backlight` | `backlight: display_backlight` in `dial_ui` config (`cv.use_id(light.LightState)`); component calls `make_call().set_brightness(...)` |
| Buzzer volume, click | `output` `buzzer` (ledc) | `buzzer: buzzer` (`cv.use_id(output.FloatOutput)`) |
| Power off | HOLD GPIO46 | `power_hold: hold_output` (`output.BinaryOutput`); component turns it off |

Settings (luminosity, buzzer volume) persist in the component via ESPHome preferences (`core/preferences.h: global_preferences`), not in HA.

### 7.2 To Home Assistant (controller phase, not now)

Reserved shape, so the display phase does not block it: a `dial_ui` trigger `on_command` with variables `(target: std::string, value: float)`; YAML maps `target` to a `homeassistant.action`. The component never names HA actions.

## 8. LVGL build constraints that affect this contract

- ESPHome generates `lv_conf.h` from the widgets used in YAML and sets every other `LV_USE_*` to `0` (`lvgl/__init__.py: generate_lv_conf_h`). There is **no `chart` widget in ESPHome YAML** (`lvgl/widgets/` has none), so History needs `LV_USE_CHART=1` from `esphome: build_flags: [-DLV_USE_CHART=1]` (build flags are read and excluded from the "unused" list in the same function). Same for any other widget used only from C++, and for `LV_FONT_MONTSERRAT_*` unless custom fonts replace them.
- The component gets the display/screen from `lvgl::LvglComponent` (`get_disp()`, `get_screen_active()`, `lvgl/lvgl_esphome.h`) via `cv.use_id`, and must set up after it (`LvglComponent` priority is `setup_priority::PROCESSOR`).

## 9. Decisions and open items

Decided (2026-11-15): no Wi-Fi or battery icons in this iteration (no data needed for them); sensor wiring by id (§3.1); thresholds and contract limit in `dial_ui:` YAML (§3.4); history hourly over 24 h, device-only, RAM only (§5).

Open:
1. Rain-remaining and weather-warning sources (§6).
2. RTC as a time source before HA connects (§5.1).
