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
| `metric` | `temperature`, `humidity`, `air_quality` (BSEC IAQ; room metrics, fixed page order); `power`; `outdoor_temperature` | C++ enum, closed set |
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
          thresholds:            # optional, overrides the defaults for this room
            warn_high: 25
        - metric: humidity
          sensor: living_hum
```

In `to_code`, the component gets each `sensor::Sensor*` and calls `add_on_state_callback` on it (exists: `sensor/sensor.h:121`, `text_sensor/text_sensor.h:59`). Benefits: ids are checked by `esphome config`, no string `room`/`metric` arguments in lambdas, and the component sees which series exist before boot.

Decided by the user (2026-11-15). The public setter below stays as an escape hatch for a value computed in YAML (`on_value: lambda: id(ui).set_reading(...)`), not as the normal path.

### 3.2 Public C++ surface (called by the component itself or YAML lambdas)

```cpp
// Live values. NaN = missing. Room id and metric name as in YAML.
void set_reading(const char *room_id, const char *metric, float value);
void set_power(float watts);
// Weather now: wired by id like the room sensors (§6); setters are the escape hatch.
void set_weather_now(float temp, const std::string &ha_condition);
void set_rain_next_hour(const std::string &forecast_attr, const std::string &time_ref_iso);
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

Decided by the user (2026-11-15): **RAM only, hourly over 24 h**, nothing written to flash. Updated 2026-11-16: the closed hours are **backfilled from Home Assistant's hourly statistics** at boot and on each reconnect (§5.2), so the chart is full right after a reboot or OTA for sensors HA keeps statistics for.

### 5.1 Model

Per series, a fixed ring of **24 closed hourly buckets + 1 open bucket** (the current hour), each `{start_epoch, mean, count}`, aligned on wall-clock hours. 25 points ≤ the design's 60-point chart limit. RAM ≈ 25 × 12 B per series.

- Only the once-a-minute sample (below) feeds the open bucket's mean; live values update the reading in memory. So every minute weighs the same, and a sensor that changes often does not count more than a stable one (implemented in M4, `model_history.h`).
- On the hour rollover the open bucket closes and the ring shifts. An hour with no value (device off, HA disconnected, sensor `NaN` all hour) is stored as missing and drawn as `LV_CHART_POINT_NONE`, never interpolated.
- Wall clock: `time: - platform: homeassistant` (`homeassistant/time/`). Until the time is valid nothing is recorded. **[decide later]** the PCF8563 RTC could provide time before HA connects (CLAUDE.md open question on RTC); not needed for this contract.
- Because HA only sends on change, a stable sensor yields few callbacks. The component therefore samples the current value of each history series once a minute into the open bucket, so a flat hour still gets a mean. This reads the value already in memory; it is not a request to HA. While HA is disconnected, or the value is `NaN`, no sample is added.

### 5.2 Backfill from HA

- **When:** once per HA connection (at boot and after each reconnect, which also fills a gap), as soon as the clock is set. One request at a time; no answer within 30 s is logged.
- **Ids:** for each `history: true` metric, the component uses the sensor's `entity_id` when it is a `homeassistant` sensor without `attribute:` (found in the validated config at codegen), or the metric's `statistic_id:` option. Metrics without one are left out (warning at compile).
- **Trigger:** `on_history_request:` in the `dial_ui:` block gets `ids` (`std::string`, comma-separated). The YAML (`packages/ui.yaml`) runs `recorder.get_statistics` with `period: hour`, `types: mean`, `statistic_ids` rendered from `ids` and `start_time` 25 h ago, with `capture_response`. Its `response_template` compacts the answer to `[["sensor.x", [[hour_epoch, mean], ...]], ...]` (about 0.5 KB per series); `on_success` calls `id(ui).load_history(response["response"])`, `on_error` calls `id(ui).history_request_failed(error)`.
- **HA side (checked in HA core, `recorder/services.py`, `helpers/service.py`, `esphome/manager.py`):** `recorder.get_statistics` is an admin service with `supports_response: only`, returning `{"statistics": {"<id>": [{"start", "end", "mean"}, ...]}}` with ISO `start`. The admin check only applies to calls with a user; ESPHome device calls carry none. It needs "Allow the device to perform Home Assistant actions". Only entities with a `state_class` have statistics; others are simply absent from the answer. HA compiles an hour's statistics shortly after the hour ends, so the latest closed hour may still be the device's own.
- **Merge (`HourlyHistory::set_closed`):** HA's mean replaces the device's for closed hours inside the 24 h window (HA's covers the whole hour; the device's may be partial after a reboot). The open hour stays the device's own minute samples.

## 6. Weather

| Data | Source | Transport |
| --- | --- | --- |
| Outdoor temperature | `weather.<x>` attribute `temperature` | homeassistant `sensor` with `attribute:` |
| Condition | `weather.<x>` state (`sunny`, `rainy`, ...) | homeassistant `text_sensor` |
| Rain in the next hour (entity ids: `weather_entity` / `next_rain_entity` substitutions in `homedial.yaml`) | Météo-France `sensor.<city>_next_rain`, attributes `1_hour_forecast` and `forecast_time_ref` | two homeassistant `text_sensor`s with `attribute:`, wired as `weather: rain_forecast / rain_forecast_time` |
| Weather warning | not this iteration | none |
| Hourly forecast (24 h) | `weather.get_forecasts` action, `type: hourly` | request/response, see below |

Provider: **Météo-France** (decided). Its weather entity supports hourly forecasts (`meteo_france/weather.py`: `FORECAST_DAILY | FORECAST_HOURLY`).

**Rain arc: rain presence over the next 60 min** (decided; replaces the design's "rain left, 0 to 6 h").
- Source, checked in `meteo_france/sensor.py` (`MeteoFranceRainSensor`): the Next rain sensor's state is the start time of the first rainy slot (not used). Its attributes are `forecast_time_ref` (ISO time of the first slot) and `1_hour_forecast`, a dict `{"0 min": "<description>", "5 min": ..., ...}`, one entry per slot of the provider's 1-hour rain forecast. Updated every 5 min (`meteo_france/coordinator.py: SCAN_INTERVAL_RAIN`).
- The entity exists only if Météo-France supports rain forecast at the configured location (`meteo_france/__init__.py`: the rain coordinator is dropped if its first refresh fails). If it is missing, the arc is not drawn.
- Transport: HA sends the dict attribute as Python `str()` text (§3.3), e.g. `{'0 min': 'Temps sec', '5 min': 'Pluie faible', ...}`. **Parsed on the device** (decided): the component reads each `'<N> min': '<text>'` pair; a slot is dry if its text is exactly `Temps sec`, rainy otherwise. A parse failure is treated as missing (no arc), and logged. Risk accepted: a wording or format change in Météo-France or HA breaks it silently into "no arc".
- Slot N covers `[forecast_time_ref + N min, next slot's start)`, the last one up to +60 min. Slots are placed relative to the device clock, so past slots fall off as time passes between 5-min updates.
- Drawing: the 270° ring is a 0 → 60 min timeline starting at "now". Rainy slots are `accent` segments on the `track` background (one `lv_arc` per contiguous rainy run, no knob). The whole ring, track included, is hidden when every remaining slot is dry. The rain line text ("Rain for 2 h") is replaced by **[spec]** wording to agree, e.g. "Rain in 15 min" / "Rain now".

**Value semantics.**
- Weather state `unavailable`/`unknown` or a `NaN` temperature: the page shows `--` + OFFLINE like any reading.
- The temperature is in HA's unit system (the attribute and forecast are converted by HA); the device only draws `°`, matching the design ("the unit is a setting").
- The forecast's first row is the provider's first hourly slot, which can be the current or the next hour. The component drops rows whose hour has fully passed and labels the first remaining one "Now".
- "Today"/"Tomorrow" on the Forecast page need the local date: `homeassistant` time applies HA's timezone when HA sends it pre-parsed (HA 2026.3+, `api/api_connection.cpp`); otherwise the device uses the `timezone:` set in YAML.

**Condition → icon.** HA has 15 condition strings (`weather/const.py`): `clear-night`, `cloudy`, `exceptional`, `fog`, `hail`, `lightning`, `lightning-rainy`, `partlycloudy`, `pouring`, `rainy`, `snowy`, `snowy-rainy`, `sunny`, `windy`, `windy-variant`. They map to the 6 design icons by one table in C++ (decided):

| Icon | HA conditions |
| --- | --- |
| clear | `sunny`, `clear-night` |
| part cloudy | `partlycloudy`, `windy-variant` |
| cloudy | `cloudy`, `fog`, `windy`, `exceptional` |
| rain | `rainy`, `pouring` |
| snow | `snowy`, `snowy-rainy`, `hail` |
| storm | `lightning`, `lightning-rainy` |

 The condition word shown under the icon is the component's own short label per HA string (≤ 12 chars), not the raw HA string.

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
          [{{ as_timestamp(f.datetime) | int }},"{{ f.condition or '' }}",{{ f.temperature | round(1) if f.temperature is not none else 'null' }}]{{ ',' if not loop.last }}
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
- `weather.get_forecasts` is an entity service with `supports_response: only` returning `{"forecast": [...]}` per entity, so the action result is `{"weather.x": {"forecast": [...]}}` (`weather/services.py`). Rows have `datetime` (ISO string, required), `condition` and `temperature`, both possibly `None`, already converted to HA's unit system (`weather/entity.py: Forecast`, `_convert_forecast`). The template guards both `None`s.
- `type: hourly` fails with a validation error if the weather integration does not support hourly forecasts (`raise_unsupported_forecast`); the UI then shows the Forecast page as OFFLINE. **Check your provider supports hourly.**
- The template has not been run yet: try it in HA Developer tools > Template / Actions before relying on it.

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

Decided (2026-11-15): Electricity = `power: {sensor, contract_limit: 6000W}` with the entity in the `power_entity` substitution; no Wi-Fi or battery icons in this iteration (no data needed for them); sensor wiring by id (§3.1); thresholds and contract limit in `dial_ui:` YAML (§3.4); history hourly over 24 h, device-only, RAM only (§5); weather provider Météo-France, rain arc = next 60 min from the Next rain sensor parsed on the device, no weather warnings this iteration, condition → icon table (§6).

Open:
1. Rain line wording under the arc (§6).
2. RTC as a time source before HA connects (§5.1).
