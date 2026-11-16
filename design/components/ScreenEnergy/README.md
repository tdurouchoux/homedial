> **Built 2026-11-16** as the child page of Electricity (`spec.md` "Energy"). Differences from this spec: no delta against yesterday (not wanted), no cost, no limit line.

Energy shows today's consumption: the total, and the 24 hourly bars.

Consumer provides: today's total in kWh, 24 hourly values (not-yet-reached hours empty), optionally a cost. (The delta against yesterday is not built.)

- "Today" in `label` / `ink-muted` at top 24; total in `value-m` / `ink` at top 42 with its unit; BarChart at top 80; "00h" and "24h" in `caption` at top 158. Nothing below the axis labels (the delta was dropped).
- Cost, if shown, replaces nothing: put it on a second line only if there is room, written `1.92 EUR` (the `€` sign needs the custom font).
- A detail page opened from Power with a press, no PageDots; a long press returns.
