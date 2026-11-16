Sensor shows one metric of one room as an ArcGauge, with the room name above the metric name and a 24 h trend line under the value.

Consumer provides: the room name (12 characters or fewer), the metric (Temperature, Humidity, CO2 or PM2.5), the reading, its unit and state.

- From top: Wi-Fi and battery icons at top 22 (`ink-muted`, 16 px); room in `body` / `ink` at top 44, with its optional RoomIcon in front (icon and name centred as one line); metric and unit as one line in `label` / `ink-muted` at top 63 ("Temperature °C"); reading in `value-xl` at top 86; trend line at top 152; StatusLabel at top 186; PageDots at top 222.
- **Trend line:** 96 x 24, centred, the last 24 h of hourly means (the History data): `accent` line 2 px, missing hours as gaps, the latest point a 6 px `accent` dot; y range is the data padded 15 %, never flatter than 1/30 of the metric's ring range. No gridlines, axes, labels or limit line (History has those). Shown only when the metric keeps history and at least two hours have data; otherwise the spot stays empty. It redraws after each minute sample.
- The room is the strongest line after the number: the user must know which room before reading the value. Never swap the two or merge them ("Kitchen PM2.5" on one line is not allowed).
- A room's pages are, in order, Temperature, Humidity, CO2, PM2.5; skip a metric the room has no sensor for. Turn moves between them, PageDots count only this room's sensors, and a long press returns to the Room page.
- Press opens History for this metric in this room.
- Don't show a second room's value here; compare rooms in History (turn).
