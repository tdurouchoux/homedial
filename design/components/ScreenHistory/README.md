History shows one metric of one room over the last 24 hours: room, metric and range, the current value, a TrendChart, its axis ends and a StatusLabel. Touching and holding the chart scrubs through the samples.

Consumer provides: the room name, the metric, a series of up to 60 samples with their times, and the limit if there is one.

- Room in `body` / `ink` at top 28, with its optional RoomIcon in front; metric and range in `label` / `ink-muted` at top 46; current value in `value-m` at top 62; chart 160 x 74 at top 98; "-24h" and "now" in `caption` at top 174; status label at top 190.
- A detail page opened from a Sensor page, no PageDots. Swipe left and right (or turn the encoder) shows the same metric in the next room, which is how rooms are compared. Swipe down or Button A returns.
- **Scrub.** Touch and hold the chart for 250 ms, then drag. A 2 px `ink-muted` cursor line and an `accent` dot snap to the nearest sample. The metric line becomes the sample's time ("CO2 ppm · 21:00"), the value becomes that sample's value, and the status label shows that sample's state. Releasing keeps the readout for 2 s, then everything returns to the current reading. The hold is what separates scrub from swipe: a quick drag is always a swipe.
- Encoder equivalent: press starts scrub on the latest sample, turn moves the cursor one sample, press or Button A ends it.
- Keep the finger clear of the readout: value and time sit above the chart, never under the cursor.
- If the limit is exceeded in the window, draw it as the dashed `warn` line. Don't add axis numbers or a legend.
