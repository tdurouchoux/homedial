# Implementation status

All eight planned milestones are implemented and confirmed on the device by the user (2026-11-15).
The detailed milestone plan is in the git history of this file. Product decisions live in `spec.md`,
the data contract in `contract.md`, and conventions in `CLAUDE.md`.

## Done
| Milestone | Scope |
| --- | --- |
| M1 | Hardware packages, LVGL input devices (encoder, touch), HOLD, `dial_ui` skeleton |
| M2 | Rooms model, Room and Sensor pages, thresholds (temperature, humidity, BSEC IAQ) |
| M3 | Navigation (encoder, swipes, taps), light motion (dots glide, ring sweep, nudges), Settings entry |
| M4 | History: 24 h hourly means in RAM, TrendChart, encoder and touch scrub |
| M5 | Weather now (icons, next-hour rain arc) and hourly forecast (`weather.get_forecasts`) |
| M6 | Electricity (live power ring to the 6 kVA contract limit) |
| M7 | Settings: luminosity, buzzer volume and pitch, screen off, power off |
| M8 | Dim after 30 s, screen off after 60 s, wake on any input |
| Extra | Room icons, `flash_size: 8MB` |

## Open items
- History: full check over a day of uptime.
- Weather city on the Forecast title (not shown).
- RTC (PCF8563) as a time source before Home Assistant connects.
- IAQ: say when the BSEC value is not calibrated yet.
- Align `design/` with the decisions recorded in `spec.md` (motion, metrics, Settings swipe, rain arc).
- Controller phase (UI -> HA actions), see `contract.md` §7.2.

## How to verify a change
- `tests/host/run.sh`: unit tests for the LVGL-free `model_*` files.
- `uvx esphome config homedial.yaml`, then `uvx esphome compile homedial.yaml` (cloud environment `homedial`).
- The user flashes and checks on the device; nothing is claimed working on the device without that.
