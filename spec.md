# Homedial UI specification

Product decisions for the Dial UI. Look and feel, components and the input vocabulary come from the design system in `design/`; this file defines the screen graph, navigation and behaviour. Status: navigation aligned, data and settings still open (see the end).

## Pages

| Level | Page | Design component | Notes |
| --- | --- | --- | --- |
| 0 | Room summary (one per room) | ScreenRoom | 2 to 4 readings |
| 0 | Weather now | ScreenWeather | rain-remaining arc |
| 0 | Electricity | ScreenPower | live power, no children |
| 1 | Sensor (one per metric of a room) | ScreenSensor | Temperature, Humidity, and CO2 / PM2.5 only if the room has the sensor |
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
  opened from level 0 by long press / touch hold / swipe down
  Settings list --press--> Adjust luminosity | Adjust buzzer volume | Screen off | Power off
```

## Navigation rules

- One table is shared by encoder and touch; both reach every page.
- **Sibling move** (encoder turn, swipe left/right): next or previous page at the same level. Level 0 stops at both ends. On Sensor pages it moves between the room's metrics.
- **Deeper** (encoder press, swipe up, or a tap on the target): open the page's child. On a Room page, a tap on a reading opens that metric's Sensor page directly; a press opens the first metric.
- **Back** (encoder long press 600 ms, swipe down): up one level. From Settings it returns to the page you left. From an editor or a scrub it cancels.
- **Settings**: from level 0, the back gesture (long press, swipe down) or a 600 ms touch hold opens the Settings list.
- **Level 2 interaction pages** (History, Hourly forecast, Adjust): turn and swipe act only inside the page and never change page.
  - History: press starts the scrub on the latest sample, turn moves the cursor, press or long press ends it. Touch: hold 250 ms on the chart, then drag.
  - Hourly forecast: turn moves the focus one hour; drag scrolls and snaps; tap centres a column.
  - Adjust: turn changes the value by one step, press saves, long press cancels.
- Slide animation 200 ms in the direction of the gesture.

## Settings list

Rows: Luminosity, Sound, Screen off, Power off (exact wording up to 12 characters, to confirm).
- **Luminosity** and **Sound** open an Adjust page.
- **Screen off** turns the backlight off; any input wakes it.
- **Power off** drives HOLD low. This cuts power on battery only; on USB the device stays powered.

## Behaviour

- **Dimming:** the panel dims to 30% after 30 s without input and wakes on any input.
- **Alerts:** soft and visual only. The reading turns `warn` / `danger` and its status label follows. Nothing takes over the screen, nothing needs to be dismissed, and an alert does not wake the screen. It clears when the reading recovers.
- **Missing data:** a value shows `--` with the OFFLINE status (design rule).

## Changes needed in `design/` to match this spec (not applied yet)

- ScreenHistory: remove the swipe/turn to the same metric in the next room.
- ScreenAlert and the input table: replace the takeover and mute by soft alerts (status colour).
- ScreenEnergy: drop (or mark as future).
- Input table: swipe down at level 0 opens Settings; the settings jump is by long press, touch hold or swipe down.
- ListMenu rows and a Settings page: add the four rows above.

## Open questions

- Home Assistant entities per room and metric, thresholds per metric, contract limit for the power ring, weather source (hourly forecast and rain remaining).
- Where the weather city, the contract limit and the thresholds are configured: device settings, YAML or Home Assistant.
- Language (English with stock ASCII fonts, or French with Latin-1 custom fonts) and theme (dark only, or dark and day).
- Whether an alert should also wake the screen.
