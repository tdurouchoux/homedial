Weather now shows the outdoor temperature, the condition as an icon and a word, and today's low and high.

Consumer provides: temperature, condition code (one of six), today's low and high, optional weather warning.

- The ring is today's range: it runs from the low (lower left) to the high (lower right) and the knob marks now. It is `accent`, not a state colour; weather has no OK or ALERT.
- From top: WeatherIcon 48 px at top 26, temperature in `value-xl` with `°` and no unit (the unit is a setting) at top 76, condition in `label` / `ink-muted` at top 134, "L 9°   H 16°" in `label` / `ink` at top 160.
- A weather warning from the provider replaces the condition line with a StatusLabel (WARN or ALERT) and adds the Alert screen; nothing else on this screen changes colour.
- Press opens the forecast; PageDots at the bottom.
