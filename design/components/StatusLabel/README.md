The StatusLabel says the state of a reading in a word and a glyph, so the colour is never the only signal.

Consumer provides: one of four states.

- No container, border or fill: a 12 px glyph and the word in `label`, both in the state colour (OFFLINE in `ink-muted`), 6 px gap, one 16 px line. Text in the state colours holds 4.5:1 on `bg` in both themes.
- Words: OK (check), WARN (triangle), ALERT (cross), OFFLINE (dashed ring, `ink-muted`). Don't rename them or add a count.
- Place it in the opening at the bottom of the ring (top 186 px), centred. One status label per screen.
- In LVGL: an `lv_obj` with flex row layout, a symbol label and a text label.
