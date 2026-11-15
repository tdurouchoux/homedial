PageDots shows which page of the carousel is open; the encoder turn and swipe move between pages.

Consumer provides: page count (3 to 7) and the active index.

- Dots are 8 px, `line` colour; the active dot is an 18 x 8 bar in `ink`. Inactive dots stay at 3:1 on `bg`.
- Up to five pages: 8 px gaps, centred at top 222 px. Six or seven pages: 6 px gaps (102 px wide at seven), centred at top 218 px so the end dots stay inside the disc. More pages need a different pattern, not smaller dots.
- Dots show the position within the carousel being turned through. Detail pages opened from a carousel have none.
- Don't make the dots tappable (too small); the page changes by turning or swiping.
