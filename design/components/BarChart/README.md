The BarChart shows 24 hourly values as bars in the same 160 x 74 box as TrendChart; use it for energy, where each bar is an amount per hour.

Consumer provides: 24 values (or fewer, with the rest not yet reached), the axis maximum, optionally a limit and the current hour.

- Past bars `line`, the current hour `accent`, hours not reached are 2 px `track` stubs. A full past day uses `accent` for all bars.
- A limit is a dashed `warn` line, and bars above it fill `warn`: dash and colour together carry it. Bars are 62% of their slot wide, `radius-sm`-sized tops (1.5 px at this scale).
- Label axis ends in `caption` outside the plot, no value labels on bars, one series only.
- Use TrendChart (line) for a continuous reading and BarChart for quantities per period.
