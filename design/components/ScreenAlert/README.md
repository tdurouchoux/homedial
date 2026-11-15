Alert takes over the display when a reading crosses a limit: a full 360° ring in the state colour, a triangle glyph, a short headline, the room, and the value.

Consumer provides: the metric plus a verb ("PM2.5 high"), the room name, the value with its unit, and the severity (WARN or ALERT).

- Ring is a closed circle at `ring-w` in `danger` or `warn`; the triangle (52 px high, same colour) holds a `bg`-coloured "!". Headline in `title` at top 102, room in `label` / `ink-muted` at top 126, value in `value-m` at top 144, hint in `body` / `ink-muted` at top 180.
- The room is always shown: an alert that doesn't say where is not actionable. Several rooms in alert at once are queued one after the other, oldest first, with the room as the only difference.
- Only ALERT pulses (ring opacity between 1.0 and `opa-disabled`, 800 ms). WARN is steady so it never competes.
- Press or Button B mutes the pulse and returns to the previous screen; the status label on that room's Sensor page stays ALERT until the reading recovers.
- Don't flood the background with the status colour, auto-dismiss, or merge two rooms in one alert.
