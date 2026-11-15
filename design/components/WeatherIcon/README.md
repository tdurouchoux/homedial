WeatherIcon is the set of six condition glyphs (clear, part cloudy, cloudy, rain, snow, storm) at 48 px (`icon-l`) and 32 px (`icon-m`).

Consumer provides: the condition code mapped to one of the six glyphs, and the condition word beside it.

- Monochrome, `ink`, 3 px outline at 48 px (2 px at 32 px), rounded ends and joins. Drawn as a font glyph in LVGL so the text colour sets it; no colour per condition, so a sun never reads as WARN.
- Always pair with the word (Clear, Part cloudy, Cloudy, Rain, Snow, Storm): the icon alone is not enough.
- Map a provider's many codes to these six; don't add glyphs for fog, wind or hail, put those in the condition word and keep the nearest glyph.
- Part cloudy has a gap between sun and cloud in the glyph (the preview fills the cloud with `bg` to show it).
- The previews are SVG stand-ins for the glyphs you will generate from them (see LVGL notes).
