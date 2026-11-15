# Homedial UI specification

Product decisions for the Dial UI. Look and feel, components and the input vocabulary come from the design system in `design/`; this file defines the screen graph, navigation and behaviour. Status: navigation aligned, data and settings still open (see the end).

## Pages

| Level | Page | Design component | Notes |
| --- | --- | --- | --- |
| 0 | Room summary (one per room) | ScreenRoom | 1 to 3 readings |
| 0 | Weather now | ScreenWeather | rain arc = next 60 min |
| 0 | Electricity | ScreenPower | live power, no children |
| 1 | Sensor (one per metric of a room) | ScreenSensor | Temperature, Humidity, Air quality, each only if the room has the sensor |
| 2 | History (one per metric of a room) | ScreenHistory | scrub |
| 2 | Hourly forecast | ScreenForecast (hourly strip) | opened from Weather now |
| S | Settings list | ListMenu | not in the carousel |
| S+1 | Adjust luminosity, Adjust buzzer volume | ScreenAdjust | opened from Settings |

Dropped for now: Energy history (it would need on-device data storage); it may return later.

## Navigation graph

```
Carousel (level 0, turn / swipe left-right, no wrap, boots on the first room):
  Room 1 | Room 2 | ... | Weather now | Electricity

Room summary  --press / swipe up / tap a reading-->  Sensor pages (turn / swipe between metrics)
Sensor page   --press / swipe up-->                  History of that metric in that room
Weather now   --press / swipe up-->                  Hourly forecast
Electricity                                          (no children)

Settings (above the carousel):
  opened from level 0 by long press / touch hold / swipe down; closed by swipe up / long press
  Settings list --press--> Adjust luminosity | Adjust buzzer volume | Screen off | Power off | Exit
```

## Navigation rules

- One table is shared by encoder and touch; both reach every page.
- **Sibling move** (encoder turn, swipe left/right): next or previous page at the same level. Level 0 stops at both ends. On Sensor pages it moves between the room's metrics.
- **Deeper** (encoder press, swipe up, or a tap on the target): open the page's child. On a Room page, a tap on a reading opens that metric's Sensor page directly; a press opens the first metric.
- **Back** (encoder long press 600 ms, swipe down): up one level. From an editor or a scrub it cancels.
- **Leaving Settings** (decided 2026-11-15): Settings opens with a downward slide from above the carousel, so it closes with **swipe up** (the slide goes back up) or an encoder long press, and returns to the page you left. Swipe down does nothing on Settings. This differs from `design/README.md`, where swipe down is always back.
- **Settings**: from level 0, the back gesture (long press, swipe down) or a 600 ms touch hold opens the Settings list.
- **Level 2 interaction pages** (History, Hourly forecast, Adjust): turn and swipe act only inside the page and never change page.
  - History: press starts the scrub on the latest sample, turn moves the cursor, press or long press ends it. Touch: hold 250 ms on the chart, then drag.
  - Hourly forecast: turn moves the focus one hour; drag scrolls and snaps; tap centres a column.
  - Adjust: turn changes the value by one step, press saves, long press cancels.
- **No page-change animation** (decided 2026-11-15, after testing on the device): pages switch instantly, including opening and leaving Settings. The 200 ms slide felt laggy. Instead, two light motions (only small areas redraw): the page dots' active bar glides to the new page in 150 ms (carousel and Sensor siblings), and a Sensor page's ring sweeps from 0 to its value in 250 ms on arrival. Vertical moves nudge the arriving content the way the finger went (150 ms, ease-out): a Sensor page's value rises 12 px when going deeper; a Room page's readings come 8 px from above when going back to it (from below when leaving Settings), 30 ms apart; Settings content drops 16 px into place when it opens. Sideways moves (room to room, metric to metric) nudge the arriving content 12 px from the side the finger came from (turning clockwise counts as a swipe left): the room name and readings on a Room page, staggered 30 ms; the metric name, value and unit on a Sensor page (the room line stays). This differs from `design/README.md` ("Motion").

## Settings list

Rows (labels 12 characters or fewer): Luminosity, Buzzer vol., Screen off, Power off, Exit.
- **Luminosity** and **Buzzer vol.** (buzzer volume) open an Adjust page.
- **Screen off** turns the backlight off; any input wakes it.
- **Power off** drives HOLD low. This cuts power on battery only; on USB the device stays powered.
- **Exit** closes Settings and returns to the page you left, same as a long press or swipe down.

## Behaviour

- **Dimming and sleep:** with no input, the panel dims to 30% after 30 s and the screen turns off (backlight off) after 60 s in total. Any input wakes it to full brightness. The first input after a screen-off only wakes the screen and is not acted on.
- **Alerts:** soft and visual only. The reading turns `warn` / `danger` and its status label follows. Nothing takes over the screen, nothing needs to be dismissed, and an alert does not wake the screen. It clears when the reading recovers.
- **Missing data:** a value shows `--` with the OFFLINE status (design rule).

## Design alignment

`design/` has been updated to match this spec: soft alerts (ScreenAlert marked unused), no room-to-room swipe on History, Energy marked as a future page, Settings opened from the top level, and the sleep timings. The `preview.html` files for Alert and Energy were not changed.

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
- **History** is recorded on the device only, in RAM, as hourly means over the last 24 h. It starts empty after a reboot or update and fills over 24 h; hours without data show as gaps. No history is fetched from Home Assistant.

## Room metrics

Decided 2026-11-15: the rooms' sensors are BME688s, so the metrics are **Temperature, Humidity and Air quality** (the Bosch BSEC IAQ index, 0 to 500, labelled "Air quality" / unit "IAQ" on the Sensor page and "Air IAQ" on the Room page). This differs from `design/`, which shows CO2 and PM2.5; there is no CO2 or PM2.5 sensor. A room therefore has 1 to 3 readings, and the Room page uses the design's 2- and 3-reading layouts (1 reading is centred). The status is computed on the displayed, rounded value.

IAQ thresholds (OK up to 100, WARN 101 to 200, ALERT above 200) follow the band edges of Bosch's IAQ classification as listed in the ESPHome BSEC2 docs (0–50 excellent, 51–100 good, 101–150 lightly polluted, 151–200 moderately polluted, 201+ heavily polluted). The authoritative table is Table 6 ("IAQ classification and color-coding") in the Bosch BME688 datasheet; it has not been checked here yet. The IAQ index is unreliable until BSEC reports its accuracy as calibrated, which the UI does not show yet.

## Weather

- Source: the Météo-France integration in Home Assistant (current conditions, hourly forecast, Next rain sensor).
- **Rain arc** (differs from `design/`, which shows rain left over 0 to 6 h): the ring is a timeline of the next 60 minutes from now. Rainy slots of Météo-France's 1-hour rain forecast are drawn as accent segments; no ring when the whole hour is dry or the rain forecast is unavailable for the location.
- Condition icons: sunny, clear-night -> clear; partlycloudy, windy-variant -> part cloudy; cloudy, fog, windy, exceptional -> cloudy; rainy, pouring -> rain; snowy, snowy-rainy, hail -> snow; lightning, lightning-rainy -> storm.

## Open questions

- Home Assistant entities per room and metric (placeholders in `packages/ha_entities.yaml`), contract limit for the power ring.
- Wording of the rain line under the arc (e.g. "Rain now", "Rain in 15 min").
- Where the weather city is configured.
- Language (English with stock ASCII fonts, or French with Latin-1 custom fonts) and theme (dark only, or dark and day).
- Whether an alert should also wake the screen.
