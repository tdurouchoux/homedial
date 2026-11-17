> **Built 2026-11-16, revised 2026-11-17** as the child page of Electricity (`spec.md` "Energy"). It now mirrors ScreenHistory (last 24 h, scrub) instead of the "today" layout below, which is kept for reference. Not built: delta against yesterday, cost, limit line.

**As built:** ScreenHistory's layout and interaction with BarChart in place of TrendChart.
- "Energy" in `body` / `ink` at top 28; "kWh • 24 h" in `label` / `ink-muted` at top 46; the window's total in `value-m` / `ink` at top 62; BarChart (25 bars, -24 h to now) at top 98; "-24h" and "now" in `caption` at top 174.
- Scrub like ScreenHistory: the scrubbed bar is lit (`accent`), the label reads "kWh • 21:00" and the value is that hour's kWh.

**Original (today) layout, not built:**

Energy shows today's consumption: the total, and the 24 hourly bars.

Consumer provides: today's total in kWh, 24 hourly values (not-yet-reached hours empty), optionally a cost.

- "Today" in `label` / `ink-muted` at top 24; total in `value-m` / `ink` at top 42 with its unit; BarChart at top 80; "00h" and "24h" in `caption` at top 158.
- Cost, if shown, replaces nothing: put it on a second line only if there is room, written `1.92 EUR` (the `€` sign needs the custom font).
- A detail page opened from Power with a press, no PageDots; a long press returns.
