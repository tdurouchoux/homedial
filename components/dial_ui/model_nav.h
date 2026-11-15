#pragma once
// Navigation model (spec.md "Navigation graph" and "Navigation rules"). Pure C++, host-tested:
// every input (encoder, gesture, tap) is reduced to one of these actions, and nav_apply()
// decides the next page, so the three inputs cannot drift apart (design lvgl.md "Touch").

#include <cstddef>
#include <cstdint>

namespace esphome::dial_ui {

enum class PageKind : uint8_t { ROOM, SENSOR, HISTORY, SETTINGS };

struct Page {
  PageKind kind{PageKind::ROOM};
  size_t room{0};   // ROOM / SENSOR: room index; level-0 position for the carousel
  size_t index{0};  // SENSOR / HISTORY: reading index in the room
  bool operator==(const Page &o) const { return kind == o.kind && room == o.room && index == o.index; }
};

enum class NavAction : uint8_t { NEXT, PREV, OPEN, BACK };

// Slide matching the gesture (design: horizontal = siblings, up = deeper, down = back).
enum class Slide : uint8_t { NONE, LEFT, RIGHT, UP, DOWN };

struct NavResult {
  bool changed{false};
  Page page;
  Slide slide{Slide::NONE};
};

// Sizes the navigation needs: carousel = rooms (weather and power join in M5/M6),
// readings per room.
struct NavShape {
  size_t room_count{0};
  const size_t *readings_per_room{nullptr};
  // Bit i set: reading i of the room keeps history (YAML `history: true`), so it has a History page.
  const uint8_t *history_mask{nullptr};
};

// `before_settings` is where Settings returns to; it is updated when Settings opens.
inline NavResult nav_apply(const Page &cur, NavAction a, const NavShape &shape, Page *before_settings) {
  NavResult r;
  r.page = cur;
  switch (cur.kind) {
    case PageKind::ROOM:
      if (a == NavAction::NEXT && cur.room + 1 < shape.room_count) {  // level 0 stops at both ends
        r.page.room = cur.room + 1;
        r.slide = Slide::LEFT;
      } else if (a == NavAction::PREV && cur.room > 0) {
        r.page.room = cur.room - 1;
        r.slide = Slide::RIGHT;
      } else if (a == NavAction::OPEN && shape.readings_per_room[cur.room] > 0) {
        r.page = Page{PageKind::SENSOR, cur.room, 0};  // press opens the first metric
        r.slide = Slide::UP;
      } else if (a == NavAction::BACK) {
        *before_settings = cur;  // back at the top level opens Settings
        r.page = Page{PageKind::SETTINGS, 0, 0};
        r.slide = Slide::DOWN;
      }
      break;
    case PageKind::SENSOR: {
      size_t n = shape.readings_per_room[cur.room];
      if (a == NavAction::NEXT && cur.index + 1 < n) {
        r.page.index = cur.index + 1;
        r.slide = Slide::LEFT;
      } else if (a == NavAction::PREV && cur.index > 0) {
        r.page.index = cur.index - 1;
        r.slide = Slide::RIGHT;
      } else if (a == NavAction::OPEN && shape.history_mask != nullptr &&
                 (shape.history_mask[cur.room] >> cur.index) & 1u) {
        r.page = Page{PageKind::HISTORY, cur.room, cur.index};
        r.slide = Slide::UP;
      } else if (a == NavAction::BACK) {
        r.page = Page{PageKind::ROOM, cur.room, 0};
        r.slide = Slide::DOWN;
      }
      break;
    }
    case PageKind::HISTORY:
      // Level 2 interaction page: turn and swipe act inside it (scrub), never change page.
      if (a == NavAction::BACK) {
        r.page = Page{PageKind::SENSOR, cur.room, cur.index};
        r.slide = Slide::DOWN;
      }
      break;
    case PageKind::SETTINGS:
      if (a == NavAction::BACK) {
        r.page = *before_settings;  // returns to the page you left
        r.slide = Slide::UP;        // Settings sits above the carousel: it leaves upwards
      }
      break;
  }
  r.changed = !(r.page == cur);
  return r;
}

enum class Swipe : uint8_t { LEFT, RIGHT, UP, DOWN };

// Swipe -> action. Horizontal = siblings, up = deeper, down = back, except on Settings,
// which is opened by a downward slide and so is closed by swiping up (user, 2026-11-15).
// Returns false when the swipe does nothing on this page.
inline bool swipe_action(PageKind kind, Swipe s, NavAction *out) {
  if (kind == PageKind::SETTINGS) {
    if (s != Swipe::UP)
      return false;
    *out = NavAction::BACK;
    return true;
  }
  if (kind == PageKind::HISTORY) {  // only swipe down (back) changes page
    if (s != Swipe::DOWN)
      return false;
    *out = NavAction::BACK;
    return true;
  }
  switch (s) {
    case Swipe::LEFT:
      *out = NavAction::NEXT;
      break;
    case Swipe::RIGHT:
      *out = NavAction::PREV;
      break;
    case Swipe::UP:
      *out = NavAction::OPEN;
      break;
    default:
      *out = NavAction::BACK;
      break;
  }
  return true;
}

}  // namespace esphome::dial_ui
