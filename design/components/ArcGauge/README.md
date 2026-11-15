The ArcGauge shows one reading as a 270° ring, a hero number, a unit and a status label; it is the home screen's main element.

Consumer provides: the reading, its range (0 to 100 of the sensor's min/max), a label of 12 characters or fewer, a unit string, and the state (OK, WARN, ALERT, OFFLINE) computed from thresholds.

- Ring: `lv_arc` 232 px, rotation 135, background 0 to 270, `ring-w` 10, rounded; indicator colour is the state colour (`ok`, `warn`, `danger`), track `track`. No knob.
- Label in `label` / `ink-muted`; reading in `value-xl` / `ink`; unit in `label` / `ink-muted`; StatusLabel in the bottom opening.
- Do: animate the arc to a new value in 250 ms. Do: show `--` and OFFLINE when the sensor is silent for 3 intervals, with an empty indicator.
- Don't: colour the number itself by state, use more than five characters in the reading (use `value-l` for longer), or put anything else inside the ring.
