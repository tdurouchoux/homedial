Hourly shows the next 24 hours as a horizontally scrolling strip: hour, icon and temperature for each hour.

Consumer provides: 24 entries (now, then +1 h to +23 h), each with a condition code and a temperature.

- Strip of columns 44 px wide centred on x 120, so five are visible (the outer two are partly cut by the circle, which shows there is more). Per column from top 76: hour in `caption` ("Now", then "15h", "16h"), a 32 px WeatherIcon (`icon-m`) at top 96, the temperature in `title` at top 134.
- The centred column is the focus: its hour is `ink`, and a 24 x 2 px `accent` bar sits under its temperature. Neighbours are `opa-dim`, the next ones `opa-disabled`. No containers, no pills.
- Above the strip, at top 30, the focused hour's day in `label` / `ink-muted` ("Today", "Tomorrow", it switches at midnight). Below it, at top 180, the focused hour's condition word in `label` / `ink-muted`.
- Scrolling: drag sideways with momentum, snapping on a column centre; turning the encoder moves the focus exactly one hour; a tap on a column centres it. In LVGL, make the strip a flex row with `lv_obj_set_scroll_snap_x(strip, LV_SCROLL_SNAP_CENTER)` and `LV_OBJ_FLAG_SCROLL_ONE` off for touch, and update the focus styling on `LV_EVENT_SCROLL_END`.
- Horizontal drag scrolls here instead of switching pages: this page has no siblings, so the sibling swipe is free. Swipe down or a long press goes back; there are no PageDots (level 2).
- Opens from Weather now with a press, at "Now". Don't add precipitation, wind, humidity or a chart here.
