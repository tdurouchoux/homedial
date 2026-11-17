# Homedial UI specification

Product decisions for the Dial UI. Look and feel, components and the input vocabulary come from the design system in `design/`; this file defines the screen graph, navigation and behaviour. Status: navigation aligned, data and settings still open (see the end).

## Pages

| Level | Page | Design component | Notes |
| --- | --- | --- | --- |
| 0 | Room summary (one per room) | ScreenRoom | 1 to 3 readings |
| 0 | Weather now | ScreenWeather | rain arc = next 60 min |
| 0 | Electricity | ScreenPower | live power |
| 1 | Energy (last 24 h) | ScreenEnergy | child of Electricity; kWh per hour, scrub like History; only when `power: energy:` is configured |
| 1 | Sensor (one per metric of a room) | ScreenSensor | Temperature, Humidity, Air quality, each only if the room has the sensor |
| 2 | History (one per metric of a room) | ScreenHistory | scrub |
| 2 | Hourly forecast | ScreenForecast (hourly strip) | opened from Weather now |
| S | Settings list | ListMenu | not in the carousel |
| S+1 | Adjust luminosity, Adjust buzzer volume | ScreenAdjust | opened from Settings |

Energy history returned on 2026-11-16 as the child page of Electricity ("Energy", the last 24 h like History since 2026-11-17): the hourly figures come from Home Assistant's statistics, so the device stores nothing (see "Energy" below).

## Navigation graph

```
Carousel (level 0, turn / swipe left-right, no wrap, boots on the first room):
  Room 1 | Room 2 | ... | Weather now | Electricity

Room summary  --press / swipe up / tap a reading-->  Sensor pages (turn / swipe between metrics)
Sensor page   --press / swipe up-->                  History of that metric in that room
Weather now   --press / swipe up-->                  Hourly forecast
Electricity   --press / swipe up-->                  Energy, last 24 h (only if configured)

Settings (above the carousel):
  opened from level 0 by long press / touch hold / swipe down; closed by swipe up / long press
  Settings list --press--> Adjust luminosity | Adjust buzzer volume | Screen off | Power off | Exit
```

## Navigation rules

- One table is shared by encoder and touch; both reach every page.
- **Sibling move** (encoder turn, swipe left/right): next or previous page at the same level. Level 0 stops at both ends. On Sensor pages it moves between the room's metrics.
- **Deeper** (encoder press, swipe up, or a tap on the target): open the page's child. On a Room page, a tap on a reading opens that metric's Sensor page directly; a press opens the first metric. On a Sensor page, a tap on the value opens that metric's History, when the metric has `history: true`; on Electricity, a tap on the power value opens Energy, when it is configured (both added 2026-11-17, user).
- **Back** (encoder long press 600 ms, swipe down): up one level. From an editor or a scrub it cancels.
- **Leaving Settings** (decided 2026-11-15): Settings opens with a downward slide from above the carousel, so it closes with **swipe up** (the slide goes back up) or an encoder long press, and returns to the page you left. Swipe down does nothing on Settings. This differs from `design/README.md`, where swipe down is always back.
- **Settings**: from level 0, the back gesture (long press, swipe down) or a 600 ms touch hold opens the Settings list.
- **Energy** (level 1, no dots): an interaction page like History: turn and swipe act only inside it (scrub) and never change page; back (long press, swipe down) returns to Electricity.
- **Level 2 interaction pages** (History, Hourly forecast, Adjust): turn and swipe act only inside the page and never change page.
  - History (and Energy, same rules): press starts the scrub on the latest sample, turn moves the cursor, press or long press ends it. Touch: hold 250 ms on a point to show it; the readout stays 2 s after release. **No drag** (user, 2026-11-17): the drag never moved the cursor on the device and is not wanted; touch picks one point, the dial moves along the chart. This differs from `design/` (hold, then drag).
  - Hourly forecast: turn moves the focus one hour; drag scrolls and snaps; tap centres a column. Encoder steps and taps scroll the strip smoothly (decided after testing: an instant jump was hard to follow).
  - Adjust: turn changes the value by one step, press saves, long press cancels.
- **No page-change animation** (decided 2026-11-15, after testing on the device): pages switch instantly, including opening and leaving Settings. The 200 ms slide felt laggy. Instead, two light motions (only small areas redraw): the page dots' active bar glides to the new page in 150 ms (carousel and Sensor siblings), and a Sensor page's ring sweeps from 0 to its value in 250 ms on arrival. Vertical moves nudge the arriving content the way the finger went (150 ms, ease-out): a Sensor page's value rises 12 px when going deeper; a Room page's readings come 8 px from above when going back to it (from below when leaving Settings), 30 ms apart; Settings content drops 16 px into place when it opens. Sideways moves (room to room, metric to metric) nudge the arriving content 12 px from the side the finger came from (turning clockwise counts as a swipe left): the room name and readings on a Room page, staggered 30 ms; the metric name, value and unit on a Sensor page (the room line stays). This differs from `design/README.md` ("Motion").

## Settings list

Rows (labels 12 characters or fewer): Luminosity, Buzzer vol., Pitch, Screen off, Power off, Exit.
- **Luminosity**, **Buzzer vol.** (buzzer volume) and **Pitch** (buzzer pitch) open an Adjust page. A row's label and value must fit the 144 px inside the row (measured with the Montserrat 16 / 14 glyph widths): "Buzzer pitch 6000Hz" did not, so the row is "Pitch".
- **Screen off** turns the backlight off; any input wakes it.
- **Power off** drives HOLD low. This cuts power on battery only; on USB the device stays powered.
- **Exit** closes Settings and returns to the page you left, same as a long press or swipe down.
- **Decided 2026-11-15 (M7):**
  - Luminosity 10 to 100 % in steps of 5, default 100 %. Buzzer volume 0 to 100 % in steps of 10, default 50 %. Both are saved on the device (ESPHome preferences) and survive a reboot. Luminosity previews live on the Adjust page; cancel restores it.
  - The buzzer plays a short tick on each encoder detent and press (not on touch), at the set volume; 0 % is silent. Volume maps to PWM duty on a cubic curve (100 % = 50 % duty, the loudest): a passive buzzer barely changes loudness between 50 % and ~10 % duty, so a linear map made the setting seem to do nothing. On the buzzer's Adjust page the tick previews the value being set.
  - Touch on the list: a tap on the focused row opens it; a tap on a neighbouring row scrolls it into focus (swipe up closes Settings, so it cannot scroll the list). On Adjust, dragging the ring changes the value; OK or a press saves; long press or swipe down cancels.
  - **Buzzer pitch** (added 2026-11-15, range revised after testing): 18 values from 100 Hz to 6000 Hz, spaced roughly evenly to the ear (100, 150, 200, 250, 300, 400, 500, 600, 800, 1000, 1200, 1500, 2000, 2500, 3000, 4000, 5000, 6000), default 1000 Hz (the original sound). Above 6 kHz was unbearable on the device; M5Stack's own Dial examples tick at 8 kHz but publish no resonant frequency for the passive buzzer (GPIO3). The buzzer has its own LEDC timer (channel 2), so changing the pitch does not change the backlight PWM.
  - List rows use the 16 px `body` size, not the design's 20 px `title` ("Luminosity" and "100%" overlapped at 20 px).
  - Settings opens on its first row. Screen off and Power off first return to the page you left.
  - Power off: if the device is still running 1 s after HOLD goes low (on USB), the screen turns off instead, and HOLD goes high again on the next input, so unplugging later does not cut power by surprise.

## Behaviour

- **Dimming and sleep:** with no input, the panel dims to 30% after 30 s and the screen turns off (backlight off) after 60 s in total. Any input wakes it to full brightness. The first input after a screen-off only wakes the screen and is not acted on.
  - Implemented in M8 (2026-11-15): input = any touch press, button press or encoder turn (LVGL's own inactivity time). The dim level is 30 % or the user's luminosity if lower. While dimmed, an input restores full brightness and acts normally; from off, the first input only wakes. The delays are `dim_after` / `off_after` in `dial_ui:` (defaults 30 s / 60 s).
- **Alerts:** soft and visual only. The reading turns `warn` / `danger` and its status label follows. Nothing takes over the screen, nothing needs to be dismissed, and an alert does not wake the screen. It clears when the reading recovers.
- **Missing data:** a value shows `--` with the OFFLINE status (design rule).

## Design alignment

`design/` has been updated to match this spec: soft alerts (ScreenAlert marked unused), no room-to-room swipe on History, Energy as a child of Electricity, Settings opened from the top level, and the sleep timings. The `preview.html` files for Alert and Energy were not changed.

## Iteration scope

- The Wi-Fi and battery icons shown at the top of the Sensor page in `design/` are not displayed in this iteration.
- No weather warnings in this iteration (the design's WARN/ALERT on Weather now is not used).

## Data decisions

See `contract.md` for the ESPHome <-> UI contract.
- **Thresholds and the power contract limit** are set in the `dial_ui:` YAML block (per-metric defaults, overridable globally per metric or per room). Defaults (decided 2026-11-15):

  | Metric | Ring range | WARN | ALERT |
  | --- | --- | --- | --- |
  | Temperature (°C) | 10 to 40 | below 18, or from 26 | below 16, or from 30 |
  | Humidity (%) | 0 to 100 | below 40, or above 60 | below 30, or above 70 |
  | Air quality (BSEC IAQ) | 0 to 500 | 101 to 200 | above 200 |

  The cold-side temperature limits (18 / 16) were proposed and not contested; adjust them in YAML if needed.
- **History** is kept on the device, in RAM, as hourly means over the last 24 h. At boot and on each reconnect the closed hours are filled from Home Assistant's hourly statistics (decided 2026-11-16; only sensors with a `state_class` have them), so it is full right after a reboot or update; otherwise it fills over 24 h. Hours without data show as gaps.

## Room icons

Each room may set `icon:` in `packages/ui.yaml` to one of the eight design glyphs (living_room, bedroom, kitchen, bathroom, office, garage, garden, other); without it the name is centred alone. The icon shows before the room name on the Room, Sensor and History pages. With an icon, keep the name to 10 characters (design RoomIcon). The Room page shows the name at 16 px with an 18 px icon (Sensor and History: 16 px text, 16 px icon).

### Arc scales

A minimal scale sits just inside the 270° ring on the Sensor, Electricity and Weather pages (design ArcGauge, widget `ArcScale`): faint ticks and caption labels in `ink-muted`, no units.
- Sensor: ticks at min, middle and max, labelled with the metric's ring range (e.g. 10 / 25 / 40).
- Electricity: 0, half and the contract limit in W (0 / 3000 / 6000). The "limit 6000 W" caption is gone, the scale's end value shows it.
- Weather: a tick every 15 min of the next hour, labelled 15 / 30 / 45. It shows only with the rain ring and hides with it.

### Metric icons

A small coloured icon (design MetricIcon: thermometer, drop, wind, bolt) precedes the metric name on the Room (12 px), History (14 px), Sensor (16 px), Electricity "Power now" (16 px) and Energy (14 px) pages. Colours are the `metric-*` tokens; they identify the metric and never carry a status.

## Electricity

- Contract limit **6 kVA (6000 W)** (decided 2026-11-15), set as `power: contract_limit` in `packages/ui.yaml`. Ring 0 to the limit; WARN from 80 %, ALERT from 100 % (ring full).
- Live power entity: `power_entity` substitution in `homedial.yaml` (placeholder until set). Shown in W up to 9999, then kW with one decimal.
- Energy is a child of Electricity, not a carousel page, so it does not count toward the 7.
- The carousel holds at most 7 pages (rooms + Weather now + Electricity), the most the page dots fit: up to 5 rooms. `esphome config` rejects more.

## Energy

Decided 2026-11-16, revised 2026-11-17 (the earlier blocker, on-device storage, went away with the History backfill from HA statistics):
- **Page**: child of Electricity (press / swipe up), no page dots, back returns. Shown only when `power: energy:` is set in `packages/ui.yaml`.
- **Mirrors the History page** (user, 2026-11-17): same layout and the same 25-point window, -24 h to now (24 closed hours + the current one), one bar per hour of energy used. "Energy" on the title line (top 28), "kWh • 24 h" (top 46), the value (top 62), BarChart (top 98, an `lv_chart`), "-24h" / "now" (top 174). No status line.
- **Live readout**: the total of the window in kWh. **Scrub**, as on History: press starts on the latest bar, turn moves one hour, press or long press ends; touch: hold 250 ms on a bar to show it (no drag); the readout stays 2 s after release. The scrubbed bar is lit (`accent`), the line reads "kWh • 21:00" ("now" for the current hour) and the value is that hour's kWh ("--" if missing).
- **Bars**: `line`; the lit bar `accent` (the current hour when not scrubbing); hours without data 2 px `track` stubs. The axis maximum is the smallest of 1, 1.5, 2, 3, 4, 5, 6, 8 (times a power of ten) that holds the tallest bar, never below 1 kWh.
- **Numbers**: kWh with two decimals below 10 (an hour is often under 1 kWh), one up to 99.9, whole above.
- No cost, no limit line (bars are never `warn`), no change against yesterday (dropped 2026-11-16: not useful).
- **Source**: one cumulative energy counter (`energy_entity` in `homedial.yaml`, state_class `total_increasing`). Closed hours are HA's hourly `change`, converted to kWh by HA; the open hour is the live counter minus its value at the start of the hour. The live counter is scaled to kWh in `packages/ha_entities.yaml` (the user's entity is in Wh: `multiply: 0.001`; fixed 2026-11-17, bars were 1000 times too high). Details and the request in `contract.md` §5.3.
- **Offline**: the total reads "--"; the bars keep the hours already known.

## Room metrics

Decided 2026-11-15: the rooms' sensors are BME688s, so the metrics are **Temperature, Humidity and Air quality** (the Bosch BSEC IAQ index, 0 to 500, labelled "Air quality" / unit "IAQ" on the Sensor page and "Air IAQ" on the Room page). This differs from `design/`, which shows CO2 and PM2.5; there is no CO2 or PM2.5 sensor. A room therefore has 1 to 3 readings, and the Room page uses the design's 2- and 3-reading layouts (1 reading is centred). The status is computed on the displayed, rounded value.

**Sensor page trend** (decided 2026-11-16): a small 24 h trend line (the History data, hourly means) under the value, in place of the unit line; the unit joins the metric line ("Temperature °C"). Shown only for metrics with `history: true` once two hours have data; otherwise nothing. No labels or limit line: History has the detail. This differs from `design/` before this date (updated).

**Room page ring** (2026-11-16): a segmented ring around the Room page was tried (status colour, then filled by value with a sweep) and removed: room-to-room switches felt laggy (each arc part costs a full-ring pass in LVGL's software renderer). The Room page stays as in `design/` (no ring).

IAQ thresholds (OK up to 100, WARN 101 to 200, ALERT above 200) follow the band edges of Bosch's IAQ classification as listed in the ESPHome BSEC2 docs (0–50 excellent, 51–100 good, 101–150 lightly polluted, 151–200 moderately polluted, 201+ heavily polluted). The authoritative table is Table 6 ("IAQ classification and color-coding") in the Bosch BME688 datasheet; it has not been checked here yet. The IAQ index is unreliable until BSEC reports its accuracy as calibrated, which the UI does not show yet.

## Weather

- Source: the Météo-France integration in Home Assistant (current conditions, hourly forecast, Next rain sensor).
- **Rain arc** (differs from `design/`, which shows rain left over 0 to 6 h): the ring is a timeline of the next 60 minutes from now. Rainy slots of Météo-France's 1-hour rain forecast are drawn as blue segments, shaded by intensity (decided 2026-11-16): "Pluie faible" pale blue (`rain-1`), "Pluie modérée" mid blue (`rain-2`), "Pluie forte" deep blue (`rain-3`); any other rain text counts as moderate. These are rain colours, not states; when the whole hour is dry the empty track is still shown (decided 2026-11-15 after testing; the design hid it); no ring at all when the rain forecast is unavailable (location not covered, or data older than an hour).
- **Rain line** (decided 2026-11-15): "Rain now" when the current slot is rainy, "Rain in N min" when rain starts later in the hour, nothing when the hour is dry.
- Condition words (12 characters or fewer): Sunny, Clear (night), Part cloudy, Windy, Cloudy, Fog, Exceptional, Rain, Heavy rain, Snow, Sleet, Hail, Storm. Temperature in whole degrees with "°".
- Missing weather data shows `--` and OFFLINE in place of the condition word. The hourly forecast is fetched from Home Assistant on connect, every 30 minutes and when the Forecast page opens with older data.
- Condition icons: sunny, clear-night -> clear; partlycloudy, windy-variant -> part cloudy; cloudy, fog, windy, exceptional -> cloudy; rainy, pouring -> rain; snowy, snowy-rainy, hail -> snow; lightning, lightning-rainy -> storm.

## Open questions

- Home Assistant entities per room and metric (placeholders in `packages/ha_entities.yaml`).
- Where the weather city is configured.
- Language (English with stock ASCII fonts, or French with Latin-1 custom fonts) and theme (dark only, or dark and day).
- Whether an alert should also wake the screen.
