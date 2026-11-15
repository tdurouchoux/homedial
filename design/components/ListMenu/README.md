The ListMenu is the settings list the encoder scrolls: three rows visible, the focused one in the middle. Rows can be a value editor (opens Adjust) or an action (for example Screen off, Power off, Exit).

Consumer provides: row labels (12 characters or fewer) and, per row, a current value of at most 6 characters.

- Focused row: 176 x 44 (`row-h`), `radius-md`, `accent` fill, `on-accent` `title` text and value in `label`. Neighbours: `title` in `ink` at `opa-dim`, centred, no fill.
- Edit mode (after a press): `surface-2` fill, 2 px `accent` border, value in `ink` between chevrons (`LV_SYMBOL_LEFT`, `LV_SYMBOL_RIGHT`), hint "Press to save" in `label`.
- Position: a 60° scroll thumb on the right of the ring (`ring-w` 10, `accent` on `track`).
- Don't show more than three rows, nest lists more than two deep, or use accent text on `surface-2` in the day theme.
- It also works as a picker for a long series, such as many rooms: one row per item, 12 characters or fewer, the item count shown by the scroll thumb.
