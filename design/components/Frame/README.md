The Frame is the geometry every screen is laid out in: a 240 px disc with a bezel ring, a content circle and a safe box.

Consumer provides: a 240 x 240 `lv_screen` filled with `bg`.

- Draw arcs in the ring (r 106 to 116). Put text, dots and lists inside the content circle (r 98).
- Rectangles must fit the safe box, or be narrower than the chord at their height (table in the preview).
- The open 90° at the bottom of a 270° gauge (below y 190) is the place for a status label and page dots; keep both within 100 px of width.
- Don't place text across the ring, and don't assume a square: the corners of the 240 x 240 buffer are never seen.
