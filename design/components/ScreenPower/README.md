Power shows live consumption in watts as an ArcGauge scaled to the contract limit.

Consumer provides: instantaneous power in W, the contract limit in W (set once in settings), and the update rate (1 Hz).

- Ring 0 to limit; indicator `ok` below 80%, `warn` from 80%, `danger` at 100% and above (ring full). Status label OK, WARN or ALERT. These thresholds are fixed so every consumption screen reads the same.
- Reading in `value-xl` (four digits fit; above 9999 W switch to kW with one decimal), unit `W` in `label`, "limit 6000 W" in `caption` / `ink-muted` at top 158.
- Same layout as ArcGauge; this is the Consumption kind of metric (see Metric types). Electricity's only child is Energy (`ScreenEnergy`, last 24 h), opened with a press or swipe up.
- Don't show the euro cost here; it would belong to the Energy page (not shown there either for now).
