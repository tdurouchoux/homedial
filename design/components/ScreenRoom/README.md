Room shows one room's readings, two to four of them, with the room name on top; it is a page in the carousel you turn through.

Consumer provides: the room name (12 characters or fewer) and, for each sensor the room has, the reading and its state. Readings always come in the order Temperature, Humidity, CO2, PM2.5; a room without a sensor for a metric just has fewer readings.

- Layout by count, nothing else on screen: **2 readings** side by side (centres x 70 and 170, `value-l`, label at top 89); **3 readings** one centred on top (x 120, top 60) and two below (x 78 and 162, top 126), `value-m`; **4 readings** two rows of two (x 78 and 162, tops 60 and 126), `value-m`. Each reading is its metric and unit in `label` / `ink-muted` over the value.
- Room name in `label` / `ink` at top 30, with its optional RoomIcon (16 px, 6 px gap) in front, icon and name centred as one line; PageDots at top 222. No ring, no separator lines, no status words. With no ring on this page, the header may use the full disc down to r 112.
- State is the colour of the value alone: `ink` when OK, `warn` for WARN, `danger` for ALERT. An offline sensor shows `--` in `ink-muted` and keeps its place. The state spelled out in words is one tap away, on the Sensor page.
- Touch: each reading is a target (its half or third of the content circle, never smaller than 82 x 58 px); tapping opens that metric's Sensor page. Pressed state: `surface-2` fill, `radius-md`. Encoder press or swipe up opens the room's Sensor pages from Temperature.
- Swipe left and right (or turn) moves to the next room; long press opens settings from here (top level).
- Don't show a second room, add icons or trends, or shrink the type to fit a fifth reading.
