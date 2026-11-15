Adjust edits one value (brightness, alert limit) by turning the encoder; the whole ring becomes the slider.

Consumer provides: the setting name, its range and step, and the current value.

- Ring in `accent` (a setting is a choice, not a state), with a 20 px `ink` knob outlined in `bg`; label `label` / `ink-muted`; value `value-xl` / `ink`; unit `label` / `ink-muted`.
- A primary button at top 176: `accent` fill, `on-accent` `label` "OK", 84 x 32 px, `radius-sm`; its touch area extends to 44 px high. Pressing the encoder or tapping it saves; a 600 ms long press cancels (back).
- One detent changes the value by one step; accelerate after four fast detents.
- Don't use state colours here, show more than one value, or change a value live without a way to cancel.
