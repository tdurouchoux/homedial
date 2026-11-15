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
static const uint8_t HISTORY[] = {0b001, 0b11};  // room 0: only reading 0 has history
static const NavShape SHAPE{2, READINGS, HISTORY};

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
  // Deeper from a Sensor page opens History only when the reading keeps history.
  r = go(s, NavAction::OPEN, &before);
  CHECK(r.page == (Page{PageKind::HISTORY, 1, 0}) && r.slide == Slide::UP);
  CHECK(go(Page{PageKind::SENSOR, 0, 0}, NavAction::OPEN, &before).changed);
  CHECK(!go(Page{PageKind::SENSOR, 0, 1}, NavAction::OPEN, &before).changed);
  // History: only back changes page, to the same Sensor page.
  Page hist{PageKind::HISTORY, 1, 1};
  CHECK(!go(hist, NavAction::NEXT, &before).changed);
  CHECK(!go(hist, NavAction::PREV, &before).changed);
  CHECK(!go(hist, NavAction::OPEN, &before).changed);
  r = go(hist, NavAction::BACK, &before);
  CHECK(r.page == (Page{PageKind::SENSOR, 1, 1}) && r.slide == Slide::DOWN);
  r = go(Page{PageKind::SENSOR, 1, 1}, NavAction::BACK, &before);
  CHECK(r.page == room1 && r.slide == Slide::DOWN);

  // Back at the top level opens Settings; back from Settings returns to the page left.
  r = go(room1, NavAction::BACK, &before);
  CHECK(r.page.kind == PageKind::SETTINGS && before == room1);
  Page settings = r.page;
  CHECK(!go(settings, NavAction::NEXT, &before).changed);
  CHECK(!go(settings, NavAction::OPEN, &before).changed);
  r = go(settings, NavAction::BACK, &before);
  CHECK(r.page == room1 && r.slide == Slide::UP);  // Settings leaves upwards

  // Swipes: Settings closes with swipe up only; elsewhere up = deeper, down = back.
  NavAction a;
  CHECK(swipe_action(PageKind::SETTINGS, Swipe::UP, &a) && a == NavAction::BACK);
  CHECK(!swipe_action(PageKind::SETTINGS, Swipe::DOWN, &a));
  CHECK(!swipe_action(PageKind::SETTINGS, Swipe::LEFT, &a));
  CHECK(swipe_action(PageKind::ROOM, Swipe::UP, &a) && a == NavAction::OPEN);
  CHECK(swipe_action(PageKind::ROOM, Swipe::DOWN, &a) && a == NavAction::BACK);
  CHECK(swipe_action(PageKind::SENSOR, Swipe::LEFT, &a) && a == NavAction::NEXT);
  CHECK(swipe_action(PageKind::SENSOR, Swipe::RIGHT, &a) && a == NavAction::PREV);
  CHECK(swipe_action(PageKind::HISTORY, Swipe::DOWN, &a) && a == NavAction::BACK);
  CHECK(!swipe_action(PageKind::HISTORY, Swipe::LEFT, &a));
  CHECK(!swipe_action(PageKind::HISTORY, Swipe::UP, &a));

  // With Weather: carousel = room 0, room 1, Weather now.
  NavShape w = SHAPE;
  w.has_weather = true;
  Page weather{PageKind::WEATHER, 0, 0};
  r = nav_apply(room1, NavAction::NEXT, w, &before);
  CHECK(r.page == weather && r.slide == Slide::LEFT);
  CHECK(!nav_apply(weather, NavAction::NEXT, w, &before).changed);  // last page (Electricity: M6)
  r = nav_apply(weather, NavAction::PREV, w, &before);
  CHECK(r.page == room1);
  r = nav_apply(weather, NavAction::OPEN, w, &before);
  CHECK(r.page.kind == PageKind::FORECAST && r.slide == Slide::UP);
  Page forecast = r.page;
  CHECK(!nav_apply(forecast, NavAction::NEXT, w, &before).changed);
  CHECK(!nav_apply(forecast, NavAction::OPEN, w, &before).changed);
  r = nav_apply(forecast, NavAction::BACK, w, &before);
  CHECK(r.page == weather && r.slide == Slide::DOWN);
  r = nav_apply(weather, NavAction::BACK, w, &before);  // top level: Settings, returns to Weather
  CHECK(r.page.kind == PageKind::SETTINGS && before == weather);
  CHECK(carousel_size(w) == 3 && carousel_index(weather, w) == 2 && carousel_index(room1, w) == 1);
  CHECK(!nav_apply(room1, NavAction::NEXT, SHAPE, &before).changed);  // no weather: room 1 is last
  CHECK(swipe_action(PageKind::FORECAST, Swipe::DOWN, &a) && a == NavAction::BACK);
  CHECK(!swipe_action(PageKind::FORECAST, Swipe::LEFT, &a));
  CHECK(swipe_action(PageKind::WEATHER, Swipe::UP, &a) && a == NavAction::OPEN);

  // With Weather and Electricity: room 0, room 1, Weather now, Electricity.
  NavShape wp = w;
  wp.has_power = true;
  Page power{PageKind::POWER, 0, 0};
  r = nav_apply(weather, NavAction::NEXT, wp, &before);
  CHECK(r.page == power && r.slide == Slide::LEFT);
  CHECK(!nav_apply(power, NavAction::NEXT, wp, &before).changed);  // last page, no wrap
  CHECK(!nav_apply(power, NavAction::OPEN, wp, &before).changed);  // no children
  CHECK(nav_apply(power, NavAction::PREV, wp, &before).page == weather);
  r = nav_apply(power, NavAction::BACK, wp, &before);
  CHECK(r.page.kind == PageKind::SETTINGS && before == power);
  CHECK(carousel_size(wp) == 4 && carousel_index(power, wp) == 3);
  // Electricity without Weather follows the rooms directly.
  NavShape p_only = SHAPE;
  p_only.has_power = true;
  CHECK(nav_apply(room1, NavAction::NEXT, p_only, &before).page == power);
  CHECK(carousel_index(power, p_only) == 2);

  if (failures == 0)
    std::printf("All nav tests passed\n");
  return failures == 0 ? 0 : 1;
}
