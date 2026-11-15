LocationLabel is the room name shown with a sensor reading: always first, always at the same height on a given screen.

Consumer provides: the room name as set by the user, at most 12 characters when set (the device cuts anything longer).

- Style per screen: Sensor `body` / `ink` at top 44; Room `label` / `ink` at top 30; History `body` / `ink` at top 28; Alert `label` / `ink-muted` at top 126 (the headline is the first line there). The metric name is always the next line, `label` / `ink-muted`.
- One line, centred; an optional RoomIcon (16 px, 6 px gap) goes in front of the name, and a room without one is just the centred name (Alert never shows the icon). In LVGL: `lv_label_set_long_mode(label, LV_LABEL_LONG_DOT)` on a label 120 px wide, so a long name ends in an ellipsis. Two rooms must differ in their first 9 characters ("Bedroom 1", "Bedroom 2").
- Accented names ("Salle de bain", "Cuisine d'été") need the Latin-1 custom fonts; with only the stock fonts the accent is replaced by a missing-glyph box.
- Don't abbreviate a room differently on different screens, and don't show the location of weather as a room (weather has a city, set in settings, shown only on the Forecast title).
