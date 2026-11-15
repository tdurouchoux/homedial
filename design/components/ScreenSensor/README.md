Sensor shows one metric of one room as an ArcGauge, with the room name above the metric name.

Consumer provides: the room name (12 characters or fewer), the metric (Temperature, Humidity, CO2 or PM2.5), the reading, its unit and state.

- From top: Wi-Fi and battery icons at top 22 (`ink-muted`, 16 px); room in `body` / `ink` at top 44, with its optional RoomIcon in front (icon and name centred as one line); metric in `label` / `ink-muted` at top 63; reading in `value-xl` at top 86; unit in `label` / `ink-muted` at top 142; StatusLabel at top 186; PageDots at top 222.
- The room is the strongest line after the number: the user must know which room before reading the value. Never swap the two or merge them ("Kitchen PM2.5" on one line is not allowed).
- A room's pages are, in order, Temperature, Humidity, CO2, PM2.5; skip a metric the room has no sensor for. Turn moves between them, PageDots count only this room's sensors, and a long press returns to the Room page.
- Press opens History for this metric in this room.
- Don't show a second room's value here; compare rooms in History (turn).
