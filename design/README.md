Dialed is the interface system for a 240 x 240 px round sensor monitor drawn with LVGL 9. Build screens only from the tokens, components and screen previews here. Implementation notes are in the LVGL section; constants for C code are in `lvgl/ui_tokens.h`.

## Content fundamentals
- Show one reading per screen, or two to four on the Room screen. Never more.
- Every sensor reading names its room first, then the metric: "Living room" above "Temperature". Room names are set by the user, 12 characters or fewer, sentence case. Longer names are cut with an ellipsis (`LV_LABEL_LONG_DOT`), so two rooms must differ in their first 9 characters. Weather has a city instead of a room, shown only on the Forecast title. A room may also have an optional icon (RoomIcon) shown before its name; without one the name is centred alone.
- Sentence case for names and hints ("Temperature", "Press to save"). Status words are upper case: OK, WARN, ALERT, OFFLINE.
- Labels are 12 characters or fewer. Write units as plain ASCII next to the label or under the reading: `°C`, `%`, `ppm`, `ug/m3`, `W`, `kW`, `kWh`, `km/h`, `EUR`. The stock LVGL Montserrat fonts hold ASCII and `°` only: no `µ`, `³`, `₂`, `€` and no accented letters. A French interface needs custom fonts with Latin-1 (see LVGL notes); French strings run about 20% longer, so shorten the words, never the type size.
- A missing value is `--`, paired with the OFFLINE status label. Never show a stale number without saying so.
- Hints are a verb plus the input: "Press to save", "Hold to go back". No emoji, no exclamation marks, no sentences longer than 3 words on the device.

## Metric types
Every metric is one of three kinds. The kind decides what the ring means and when colour is allowed.

| Kind | Examples | Ring | Colour |
| --- | --- | --- | --- |
| Status | Temperature, humidity, CO2, PM2.5 (each in a room) | Fills 0 to 100% of the sensor range | Ring and status label follow `ok`, `warn`, `danger` |
| Consumption | Power now; energy today (future) | Power: 0 to the contract limit. Energy (future page): 24 hourly bars, no ring | Power turns `warn` at 80% of the limit and `danger` at 100%. Energy bars stay neutral (`line`, `accent`) except bars above a limit, which are `warn` with a dashed line |
| Information | Weather now, hourly forecast | Weather now: arc = rain left (0 to 6 h), when raining | `accent` only. A provider's weather warning is a WARN or ALERT status label, nothing else on weather is a state colour |

Weather and power (and energy, once added) follow the anatomy of the Sensor page (names on top, hero value, unit or condition word, one status or hint line, PageDots below), without a room line.

## Visual foundations
- **The disc.** The visible area is a circle of radius 120. Draw the bezel arc at the edge, then keep all content inside the content circle (`content-r` 98). A rectangle (chart, list) must fit the `safe-box` of 160 x 160, or be narrower than the chord at its height: `2·√(98² − d²)`, where d is its distance from the centre (d 40 → 179 px, d 60 → 155, d 80 → 113, d 90 → 78).
- **Ground.** Every screen is `bg`. Use `surface-2` for rows. Do not paint a screen with a status colour; the status colour lives on the ring and the status label (glyph and word).
- **Colour roles.** `accent` means "you are here / you can change this" (focus, edit, selection, chart line, primary button). `ok`, `warn`, `danger` mean the state of a reading and only that. Status is never colour alone: each carries a word or glyph, and `ok` is blue, so it is distinct from `danger` for red-green colour blindness. The one exception is the Room page, where the colour of the value is the whole status (`ink` when OK, `warn`, `danger`); the Sensor page one tap away spells the state out.
- **Text colours.** Readings and titles in `ink`; labels, units and hints in `ink-muted`; text on an accent fill in `on-accent`. Do not put `accent` text on `surface-2` in the day theme.
- **Type.** One family (Montserrat, regular only), seven sizes that exist as stock LVGL fonts: `value-xl` 48, `value-l` 40, `value-m` 28, `title` 20, `body` 16, `label` 14, `caption` 12. Hierarchy comes from size and `ink` vs `ink-muted`, not weight. `caption` only for axis ends and state words.
- **Arcs.** The gauge sweeps 270°, from 135° to 45° (LVGL rotation 135, background angles 0 to 270), rounded caps, width `ring-w`, outer edge `ring-inset` from the edge. The open 90° at the bottom holds the status label and page dots.
- **Shape and depth.** Flat. No shadows, blur or gradients (they cost frame time and band at 16-bit colour). Rows `radius-md`; buttons `radius-sm`; no pill or capsule shapes; separation by fill (`surface-2` on `bg`) or a 2 px border.
- **Spacing.** Use `space-1` to `space-6` (4 to 32 px). Rows are `row-h` 44 px; the status label is one 16 px line; touch targets at least `touch-min` 40 px.
- **States.** Focus: accent-filled row with `on-accent` text. Editing: `surface-2` row with a 2 px `accent` border and chevrons around the value. Pressed: `opa-pressed`. Unfocused neighbours: `opa-dim`. Disabled: `opa-disabled`.
- **Motion.** Keep every animation at or under 250 ms: arc value `lv_anim_path_ease_out`, page change a 200 ms horizontal slide. Nothing else moves.

## Input
Design every screen for all three inputs; none may be required alone. The hardware has **one** physical button (on the M5Stack Dial it is the encoder push), so the encoder alone (turn, press, long press) must reach every screen and every action. A second button is optional and only adds shortcuts; no function may depend on it. Touch and the encoder overlap on purpose: touch is quicker for jumping (a reading, a swipe), the encoder is the precise and eyes-free path, and both must reach every screen.

| Input | Action |
| --- | --- |
| Encoder turn | Previous/next page; in a list, move focus; in an editor, change the value by one step (accelerate on fast turns). |
| Encoder press (the one button) | Select the focused row; confirm an edit. |
| Encoder press on History | Start scrub on the latest sample; turn moves the cursor one sample; press or long press ends it. |
| Encoder long press (600 ms) | Back, one level up (from an editor: cancel; from a scrub: end it). At the top level (the carousel) there is nothing to go back to, so it opens the Settings list; from Settings it returns to the page you left. |
| Optional second button | Shortcut only: short press = back, same as long press; never required. |
| Tap | Same as press on the tapped row or control; targets are 40 px or larger. On a Room page, a tap on a reading opens that metric's Sensor page directly. |
| Swipe left or right | Next or previous sibling at the same depth: the next page of a carousel, the next item of a series. On a page that is itself a scrolling strip (hourly forecast) it scrolls the strip instead. |
| Swipe up | Go deeper: open the detail of the current page, same as press. |
| Swipe down | Back one level, same as long press. At the top level it opens the Settings list. |
| Touch and hold 250 ms on a chart, then drag | Scrub: a cursor follows the finger and the readout shows that sample's time and value. Release keeps it for 2 s. |
| Touch and hold 600 ms anywhere else | Open the Settings list (same as long press at the top level). |


**Navigation.** Every input uses one vocabulary: horizontal moves between siblings, up goes deeper, down goes back, and the page slide follows the gesture (200 ms). Which page opens which is defined in the project, not in this system; keep it as one table that touch and encoder both use.

## Iconography
Use the LVGL symbol glyphs that ship inside the Montserrat fonts: `LV_SYMBOL_WIFI`, `LV_SYMBOL_BATTERY_3`, `LV_SYMBOL_OK`, `LV_SYMBOL_WARNING`, `LV_SYMBOL_CLOSE`, `LV_SYMBOL_LEFT`, `LV_SYMBOL_RIGHT`, `LV_SYMBOL_SETTINGS`. They render in the label colour at 14 or 16 px. The previews here draw close SVG stand-ins (2 px strokes, rounded ends), not the font glyphs themselves. Status glyph shapes: check = OK, triangle = WARN, cross = ALERT, dashed ring = OFFLINE. Weather conditions use the six-glyph set in WeatherIcon (clear, part cloudy, cloudy, rain, snow, storm), a custom icon font in `ink`, always next to the condition word. Rooms may have an optional 16 px icon from the eight-glyph RoomIcon set, in `ink`, always next to the room name.
