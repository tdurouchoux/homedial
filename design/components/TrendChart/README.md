The TrendChart is a 160 x 74 line chart of the last 24 hours (or the last N samples) with a marker on the latest value.

Consumer provides: up to 60 samples, the axis range, and optionally a limit value.

- Size: `safe-box` wide (160), 74 tall; centred horizontally and placed so its widest rows sit near the vertical centre of the disc.
- Line `accent`, width 2; gridlines `track`, 1 px, three; the last point a 4 px dot with a `bg` outline.
- A limit is a dashed `warn` line (dashes carry the meaning, not just colour). Label axis ends with `caption`: "-24h" left, "now" right.
- Don't colour line segments by state, add axis numbers inside the plot, or use more than one series; show a second series on its own page.
- With no data, show only the dashed baseline and put the OFFLINE status label elsewhere on the screen.
- Scrub: while the user holds and drags, draw a 2 px `ink-muted` vertical cursor and an `accent` dot on the nearest sample (the last-point dot stays). The chart itself carries no value text; the screen's readout shows it. In LVGL use `lv_chart_add_cursor` and `lv_chart_get_pressed_point`.
