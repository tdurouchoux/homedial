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

### 3.1 Wiring (recommended): `dial_ui` references sensors by id

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

**[decide]** This differs from `CLAUDE.md` ("`on_value` calls a generic method"). Both can coexist: the component also exposes the public setter below, so a YAML `on_value: lambda: id(ui).set_reading(...)` still works for odd cases (a value computed in YAML). Recommendation: `use_id` as the default, setter as escape hatch.

### 3.2 Public C++ surface (called by the component itself or YAML lambdas)

```cpp
// Live values. NaN = missing.
void set_reading(Room room, Metric metric, float value);
void set_power(float watts);
void set_weather_now(float temp, const std::string &ha_condition, int rain_minutes, WeatherWarning warning);
// History / forecast payloads from homeassistant.action responses (§5, §6).
void load_history(SeriesId series, JsonArrayConst rows);
void load_forecast(JsonArrayConst rows);
void history_request_failed(SeriesId series, const std::string &error);
```

`JsonArrayConst` is what `on_success` exposes: the action trigger passes `JsonObjectConst response` (`api/__init__.py`, `homeassistant_service_to_code`; `api/homeassistant_service.h:68`).

### 3.3 Value semantics (verified)

- `platform: homeassistant` sensor publishes `NAN` when the HA state does not parse as a number (`unavailable`, `unknown`): `homeassistant/sensor/homeassistant_sensor.cpp`.
- `attribute:` is supported on homeassistant sensors and text sensors (`homeassistant/__init__.py: HOME_ASSISTANT_IMPORT_SCHEMA`). HA sends attributes as `str(value)` (`esphome/manager.py: _send_home_assistant_state`), so scalars work (`temperature` of a `weather.*` entity) but **lists arrive as Python repr, not JSON**: never read forecasts or history through an attribute.
- HA only sends on change, and resends every subscribed state when the API reconnects.

### 3.4 Status (ok / warn / danger) is computed in the component

From thresholds in the config (per metric, overridable per room), and for power from `contract_limit_w` (80 % / 100 %, design rule). YAML never sends a state. **[decide]** with the spec's open question on where thresholds live; this contract assumes `dial_ui:` YAML config until the user picks otherwise.

## 4. Connectivity and staleness

A value that does not change is never resent, so a timeout per sensor would wrongly mark stable sensors OFFLINE. Instead:

- **Source of truth:** `api::global_api_server->is_connected_with_state_subscription()` (`api/api_server.h:215`; same as the YAML condition `api.connected: {state_subscription_only: true}`). This is true only for a client that subscribed to HA states (i.e. Home Assistant), not for a logs client.
- The component polls it in `loop()`. On **true → false**: all HA-fed series are marked stale (OFFLINE, `--`). On **false → true**: values come back by themselves (HA resends), and the history backfill queue is re-run (§5).
- `NaN` from one sensor marks only that series OFFLINE.

## 5. History

Only series with `history: true` keep history (the spec's History pages: room metrics). Power has none for now (Energy page dropped).

### 5.1 Model

Per series, a fixed ring of **24 hourly buckets + 1 open bucket** (the current hour), each `{start_epoch, mean, count}`, aligned on wall-clock hours. 25 points ≤ the design's 60-point chart limit. RAM ≈ 25 × 12 B per series. Wall clock comes from `time: - platform: homeassistant` (`homeassistant/time/`); until time is valid, history neither records nor requests.

- **Live tail (device):** each live value updates the open bucket's running mean. When the hour rolls over, the bucket closes and the ring shifts.
- **Backfill (Home Assistant):** closed buckets come from HA long-term statistics, so the chart is full right after boot, OTA or a disconnect, without writing anything to flash.

**[decide]** resolution: hourly (simple, matches HA's `hour` statistics). A 30-min chart (48 points) is possible with `period: 5minute` and averaging in the template; not proposed now.

### 5.2 Backfill request / response

The component cannot call `homeassistant.action` itself without touching API internals, so it fires a trigger declared in its config, and YAML performs the action:

```yaml
dial_ui:
  on_history_request:            # variables: series (int), statistic_id (std::string), start (std::string, ISO UTC)
    - homeassistant.action:
        action: recorder.get_statistics
        data_template:
          statistic_ids: "{{ [sid] }}"
          start_time: "{{ start }}"
          period: hour
          types: "{{ ['mean'] }}"
        variables:
          sid: return statistic_id;
          start: return start;
        capture_response: true
        response_template: >-
          [{%- for r in (response.statistics.values() | list | first or []) -%}
          [{{ as_timestamp(r.start) | int }},{{ r.mean | round(2) }}]{{ ',' if not loop.last }}
          {%- endfor -%}]
        on_success:
          - lambda: id(ui).load_history(series, response["response"]);
        on_error:
          - lambda: id(ui).history_request_failed(series, error);
```

Checked against the sources:
- `capture_response`, `response_template`, `on_success` (with `response`), `on_error` (with `error`) exist; `capture_response` requires `on_success` (`api/__init__.py: _validate_response_config`). Trigger args are copied, so `series` survives until the response arrives (`synchronous=False` comment).
- HA renders `response_template` with `response` = the action result and sends back `{"response": <rendered>}` as JSON (`esphome/manager.py: _handle_service_call_with_response`). Hence `response["response"]` in the lambda, a list of `[epoch, mean]`.
- `recorder.get_statistics` exists, `supports_response: only`, fields `statistic_ids`, `start_time`, `end_time`, `period` (`5minute`/`hour`/...), `types` (`mean`, ...), returns `{"statistics": {id: [{start, end, mean, ...}]}}` (`recorder/services.yaml`, `recorder/services.py`).
- It is an admin-only action, but the admin check only applies when the call has a `user_id` (`helpers/service.py: _async_admin_handler`); device calls carry none, so it passes.
- Response size limit: one API message ≤ 32 KiB on ESP32 (`api/api_frame_helper.h: MAX_MESSAGE_SIZE`). 24 rows ≈ 0.5 KB.
- Requires **"Allow the device to perform Home Assistant actions"** in the HA device options (`CONF_ALLOW_SERVICE_CALLS` in `manager.py`), otherwise HA raises a repair issue and the call is rejected.
- **Not verified yet:** the exact `data_template` syntax for list values (`statistic_ids`, `types`) and whether `on_history_request` variables can be passed into `variables:` this way. Check with `uvx esphome config` when implementing; fall back to `data:` with a lambda if needed.

**Rules:**
- `statistic_id` defaults to the `entity_id` of the series' homeassistant sensor (resolved in the component's Python validation), or can be set explicitly per metric.
- The component sends **one request at a time** (queue), on: time first valid, API reconnect, and an hour rollover after a gap. Not on page open: the ring is always current.
- `load_history` replaces closed buckets older than the open one; it never overwrites the open bucket (HA statistics only exist for finished periods).
- HA prerequisite: the sensor must have a `state_class` (e.g. `measurement`), otherwise HA keeps no long-term statistics and the response is empty. The device then shows only what it recorded live since boot (chart fills over time). Not an error.

## 6. Weather

| Data | Source | Transport |
| --- | --- | --- |
| Outdoor temperature | `weather.<x>` attribute `temperature` | homeassistant `sensor` with `attribute:` |
| Condition | `weather.<x>` state (`sunny`, `rainy`, ...) | homeassistant `text_sensor` |
| Rain remaining (min) | **[decide]** no standard HA entity; needs an HA template sensor or a provider entity | homeassistant `sensor` |
| Weather warning | **[decide]** provider-specific entity | homeassistant `text_sensor` |
| Hourly forecast (24 h) | `weather.get_forecasts` action, `type: hourly` | same request/response pattern as §5 (`on_forecast_request` trigger, `load_forecast`) |

HA condition strings are mapped to the 6 design icons by one table in C++. The forecast is requested on connect and every 30 min; the payload is compacted by `response_template` to `[[epoch, "condition", temp], ...]` (24 rows).

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

## 9. Open items (to move into spec.md once decided)

1. `use_id` wiring vs `on_value` lambdas (§3.1).
2. Where thresholds and the contract limit live (§3.4; also an open question in spec.md).
3. History resolution: hourly, 24 h (§5.1).
4. Do the HA sensors used for History have a `state_class`? (needed for backfill)
5. Rain-remaining and weather-warning sources (§6).
6. Battery/Wi-Fi icons on the Sensor page: which entities (Wi-Fi `wifi_signal` is local; the Dial has no documented battery gauge).
