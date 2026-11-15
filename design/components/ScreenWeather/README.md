Weather now shows the outdoor temperature and the condition as an icon and a word; while it is raining, an arc shows how much rain is left.

Consumer provides: temperature, condition code (one of six), minutes of rain remaining (0 when dry), optional weather warning.

- Rain arc: the same ring as ArcGauge (270 degrees from lower left, clockwise, 10 px wide, `track` background, `accent` indicator, `ink` knob). Scale is 0 to 6 h: 6 h or more fills the ring, and the arc shrinks as the rain ends. It is `accent`, not a state colour, because weather has no OK or ALERT. When it is dry there is no ring at all, not even the track.
- From top: WeatherIcon 48 px at top 34, temperature in `value-xl` with `°` and no unit (the unit is a setting) at top 84, condition in `label` / `ink-muted` at top 142.
- Rain line, only while it is raining: `label` / `ink` at top 172, "Rain for 2 h" (above 90 min round to the hour, otherwise "Rain for 40 min"; "Rain all day" if it lasts through the next 24 h). Don't print "No rain" when dry.
- A weather warning from the provider replaces the condition line with a StatusLabel (WARN or ALERT). A provider warning is a status label only; no separate alert screen.
- Press opens the hourly forecast; PageDots at the bottom.
