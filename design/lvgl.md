# LVGL notes

Written for LVGL 9.x. If you are on 8.x, rename `lv_obj_remove_flag` to `lv_obj_clear_flag` and check the arc and indev calls against your version.

## lv_conf.h
```c
#define LV_COLOR_DEPTH 16            /* RGB565: hex tokens are quantised on the panel */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_48 1
```
Enable only these seven sizes; each costs flash. The stock fonts hold ASCII and `°`: no accented letters (é, è, à, ç), no `€`, `µ`, `³`, `₂`. A French interface therefore needs custom fonts, one per size, built with `lv_font_conv`:
```sh
lv_font_conv --font Montserrat-Regular.ttf --size 16 --bpp 4 --format lvgl \
  -r 0x20-0x7F,0xA0-0xFF,0x20AC --no-compress -o dialed_font_16.c
```
`0xA0-0xFF` is Latin-1 (all French accents except œ, 0x153); `0x20AC` is `€`. Add the LVGL symbol range with a second `--font FontAwesome5-Solid+Brands+Regular.woff --range 61441,61448,61451,...` entry if you use `LV_SYMBOL_*`. Budget about 10 to 14 KB per size for this range at 4 bpp.

## Round display
Create a 240 x 240 screen with the `bg` colour. The panel's corners are physically absent, so nothing needs masking, but keep content inside `content-r` so nothing is clipped by the bezel.

## The ring gauge
```c
lv_obj_t *arc = lv_arc_create(scr);
lv_obj_set_size(arc, DIALED_ARC_SIZE, DIALED_ARC_SIZE);   /* 232 */
lv_obj_center(arc);
lv_arc_set_rotation(arc, 135);
lv_arc_set_bg_angles(arc, 0, 270);
lv_arc_set_range(arc, 0, 100);
lv_obj_remove_style(arc, NULL, LV_PART_KNOB);          /* gauge: no knob */
lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
lv_obj_set_style_arc_width(arc, DIALED_RING_W, LV_PART_MAIN);
lv_obj_set_style_arc_width(arc, DIALED_RING_W, LV_PART_INDICATOR);
lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
lv_obj_set_style_arc_color(arc, DIALED_TRACK, LV_PART_MAIN);
lv_obj_set_style_arc_color(arc, DIALED_OK, LV_PART_INDICATOR);  /* swap on state change */
```
For the value editor keep the knob: set its colour to `DIALED_INK`, pad 4, and leave the object clickable and in the encoder group.

## Encoder
Put every focusable object in an `lv_group_t`, assign it with `lv_indev_set_group`, and use `LV_INDEV_TYPE_ENCODER`. A list row is focusable; pressing it enters edit mode (`lv_group_set_editing`), turning changes the value, pressing again leaves. Apply the focused and edited looks with `LV_STATE_FOCUSED` and `LV_STATE_EDITED` styles.

Single button: the encoder push is the only required button. `LV_EVENT_CLICKED` (short press, under 600 ms) selects. For back, send `LV_EVENT_LONG_PRESSED` (600 ms, set with `lv_indev_set_long_press_time` on the encoder indev) to one handler that pops a screen, cancels an edit or scrub, or at the top level opens settings. Swipe down at the top level does the same. In ESPHome the push is a `binary_sensor` with `on_click` (`min_length`/`max_length`) to tell short from long. An optional second button just calls the same back handler.

## Performance
- No shadows, no blur, no gradients, no image recolour (weather icons are a font, see below); they cost frame time on a small MCU.
- Use `LV_DISPLAY_RENDER_MODE_PARTIAL` with a buffer of at least 1/10 of the screen. Update readings at 1 Hz, not per sample.
- Chart: at most 60 points, `lv_chart_set_type(LV_CHART_TYPE_LINE)`, series line width 2, point radius 0 except the last point. Gridlines use `DIALED_TRACK`.
- Reuse one `lv_style_t` per token role (`ring`, `status`, `row`, `row_focused`, ...) and add it to many objects, rather than setting local styles.
- Dim the panel to 30% after 30 s without input and turn the backlight off after 60 s; wake on any input (the waking input is not acted on). Alerts do not wake the screen.

## Weather icons
Ship the six conditions (clear, part cloudy, cloudy, rain, snow, storm) as one icon font, not as images: draw them as 48 px SVG outlines with a 3 px stroke, convert to a TTF (Fontello, IcoMoon or fontforge), then run `lv_font_conv` at 48 and 32 px, 4 bpp, one glyph per condition in the private-use area (`-r 0xE000-0xE005`). Show a glyph in an `lv_label` and colour it with the text colour (`DIALED_INK`), so a theme change needs no assets. Keep the condition word on screen too.

## Power and energy (energy is a future page)
- Live power uses the ring gauge with the range 0 to the contract limit in watts. Switch the indicator colour at 80% (`DIALED_WARN`) and 100% (`DIALED_DANGER`), and show the matching status label.
- Energy per hour is an `lv_chart` of type `LV_CHART_TYPE_BAR` with 24 points on one series. Colour bars individually in `LV_EVENT_DRAW_TASK_ADDED`: past hours `DIALED_LINE`, the current hour `DIALED_ACCENT`, bars above the limit `DIALED_WARN`; hours not yet reached are `LV_CHART_POINT_NONE`.
- Update power at 1 Hz, energy once a minute.

## Rooms and sensors
Model each reading as `{room_id, metric, value, unit, state, timestamp}` and look the room name up from one table of 12-character strings, so every screen shows the same label. Draw the room with a label of fixed width 120 and `lv_label_set_long_mode(label, LV_LABEL_LONG_DOT)`.

Build one Room page per room and one Sensor page per metric of that room from the same two screen constructors, passing the room and metric; refresh only the label and arc values when a reading arrives, don't rebuild the page. Keep the page list as an array of `{kind, room_id, metric}`; the encoder handler just moves an index within the current level and loads the screen with a 200 ms slide.

## Touch
Gestures: handle `LV_EVENT_GESTURE` on each screen and read the direction with `lv_indev_get_gesture_dir(lv_indev_active())`: `LV_DIR_LEFT` is next, `LV_DIR_RIGHT` previous, `LV_DIR_TOP` open, `LV_DIR_BOTTOM` back. Route all of them, and the encoder and buttons, through the same `nav_next/prev/open/back` functions so the three inputs can't drift apart. Load the new screen with `lv_screen_load_anim`, using `LV_SCREEN_LOAD_ANIM_MOVE_LEFT/RIGHT/TOP/BOTTOM` to match the gesture, 200 ms.

Reading selection: make each reading slot a clickable `lv_obj` (at least 82 x 58) with a `LV_STATE_PRESSED` style of `DIALED_SURFACE_2` and `DIALED_RADIUS_MD`; the click callback calls `nav_open_metric(room, metric)`.

Chart scrub: set the touch indev's long-press time to 250 ms (`lv_indev_set_long_press_time`). On the chart, `LV_EVENT_LONG_PRESSED` sets `scrubbing = true` and clears `LV_OBJ_FLAG_GESTURE_BUBBLE` on the chart so the drag isn't read as a swipe; `LV_EVENT_PRESSING` reads `lv_chart_get_pressed_point(chart)` and moves a cursor made with `lv_chart_add_cursor(chart, DIALED_INK_MUTED, LV_DIR_VER)` via `lv_chart_set_cursor_point`; `LV_EVENT_RELEASED` starts a 2 s timer that resets the readout and sets the bubble flag again. Settings on touch is a separate 600 ms timer started on `LV_EVENT_PRESSED` and cancelled on release or when a gesture is detected; don't start it when the press lands on a chart.

## Room ring segments
The Room page's segmented ring is one `lv_arc` per reading, each 232 px, centred, rotation 135, with `lv_arc_set_bg_angles(arc, start, start + sweep)` for its segment (start and sweep from the even split of 270° minus `ring-gap` between segments), `ring-w`, rounded. Each segment is a small ArcGauge (track on MAIN, value on INDICATOR, same range and states as the Sensor page); on arrival reset each to 0 and animate to its value. Cost note: LVGL's software renderer processes every arc part over its full circle's bounding box, whatever its sweep, so a Room page with 3 readings draws 6 full-ring passes on a page change. Create the arcs before the reading targets and leave them non-clickable, so taps reach the readings.

## Room icons
Room icons are glyphs of a second small icon font (16 px, 4 bpp, eight glyphs in the private-use range, for example `-r 0xE010-0xE017`), built the same way as the weather icons. Store the room's icon as an optional id (0 = none). Build the header as one flex row (`lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW)`, centred, 6 px gap, row width 120) holding an icon label and a name label with `LV_LABEL_LONG_DOT`; when the id is 0, don't create the icon label so the name stays centred.
