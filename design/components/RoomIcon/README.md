RoomIcon is an optional 16 px glyph shown before a room's name to recognise the room at a glance: living room, bedroom, kitchen, bathroom, office, garage, garden, other.

Consumer provides: an icon id per room, or none. No icon is the default; a room name must work without it.

- 16 px (`icon-s`), `ink`, 1.5 px strokes with rounded ends, 6 px gap to the name, icon and name centred together as one line.
- Shown on the Room, Sensor and History pages, in the same line as the room name.
- An icon never replaces the name and carries no state; a room with an unknown or custom icon uses "Other" (house).
- With an icon, a 12-character name leaves almost no margin on the Sensor page; keep room names to 10 characters when the room has an icon.
- In LVGL the glyphs are part of a small icon font, like the weather icons (see LVGL notes). The previews here are SVG stand-ins.
- Don't add colour, don't enlarge it above the name, and don't add more than eight glyphs without redrawing them as a set.
