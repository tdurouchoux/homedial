// Host unit tests for model_nav.h (spec.md "Navigation rules").
#include <cstdio>

#include "model_nav.h"

using namespace esphome::dial_ui;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                 \
    }                                                             \
  } while (0)

static const size_t READINGS[] = {3, 2};  // two rooms
static const NavShape SHAPE{2, READINGS};

static NavResult go(const Page &p, NavAction a, Page *before) { return nav_apply(p, a, SHAPE, before); }

int main() {
  Page before{};
  Page room0{PageKind::ROOM, 0, 0}, room1{PageKind::ROOM, 1, 0};

  // Carousel: no wrap at either end.
  CHECK(!go(room0, NavAction::PREV, &before).changed);
  NavResult r = go(room0, NavAction::NEXT, &before);
  CHECK(r.changed && r.page == room1 && r.slide == Slide::LEFT);
  CHECK(!go(room1, NavAction::NEXT, &before).changed);
  r = go(room1, NavAction::PREV, &before);
  CHECK(r.page == room0 && r.slide == Slide::RIGHT);

  // Deeper: first metric, slide up.
  r = go(room1, NavAction::OPEN, &before);
  CHECK(r.page == (Page{PageKind::SENSOR, 1, 0}) && r.slide == Slide::UP);

  // Sensor siblings stay inside the room; back returns to it.
  Page s{PageKind::SENSOR, 1, 0};
  r = go(s, NavAction::NEXT, &before);
  CHECK(r.page == (Page{PageKind::SENSOR, 1, 1}));
  CHECK(!go(Page{PageKind::SENSOR, 1, 1}, NavAction::NEXT, &before).changed);
  CHECK(!go(s, NavAction::PREV, &before).changed);
  CHECK(!go(s, NavAction::OPEN, &before).changed);  // History: M4
  r = go(Page{PageKind::SENSOR, 1, 1}, NavAction::BACK, &before);
  CHECK(r.page == room1 && r.slide == Slide::DOWN);

  // Back at the top level opens Settings; back from Settings returns to the page left.
  r = go(room1, NavAction::BACK, &before);
  CHECK(r.page.kind == PageKind::SETTINGS && before == room1);
  Page settings = r.page;
  CHECK(!go(settings, NavAction::NEXT, &before).changed);
  CHECK(!go(settings, NavAction::OPEN, &before).changed);
  r = go(settings, NavAction::BACK, &before);
  CHECK(r.page == room1);

  if (failures == 0)
    std::printf("All nav tests passed\n");
  return failures == 0 ? 0 : 1;
}
