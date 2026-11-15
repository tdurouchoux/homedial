Energy shows today's consumption: the total, the 24 hourly bars, and the change against yesterday.

Consumer provides: today's total in kWh, 24 hourly values (not-yet-reached hours empty), yesterday's total for the delta, optionally a cost.

- "Today" in `label` / `ink-muted` at top 24; total in `value-m` / `ink` at top 42 with its unit; BarChart at top 80; "00h" and "24h" in `caption` at top 158; the delta in `label` / `ink` at top 178.
- The delta is an arrow glyph plus signed percentage ("-12% vs yday"), in `ink`: less or more consumption isn't good or bad here, so no state colour.
- Cost, if shown, replaces nothing: put it on a second line only if there is room, written `1.92 EUR` (the `€` sign needs the custom font).
- A detail page opened from Power with a press, no PageDots; a long press returns.
