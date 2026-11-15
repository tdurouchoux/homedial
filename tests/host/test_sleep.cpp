// Host unit tests for model_sleep.h.
#include <cstdio>

#include "model_sleep.h"

using namespace esphome::dial_ui;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                 \
    }                                                             \
  } while (0)

int main() {
  SleepTimer t;
  CHECK(t.update(0) == SleepAction::NONE && t.state() == Backlight::ACTIVE);
  CHECK(t.update(29999) == SleepAction::NONE);
  CHECK(t.update(30000) == SleepAction::DIM && t.state() == Backlight::DIMMED);
  CHECK(t.update(45000) == SleepAction::NONE);
  CHECK(t.update(60000) == SleepAction::TURN_OFF && t.state() == Backlight::OFF);
  CHECK(t.update(0) == SleepAction::NONE && t.state() == Backlight::OFF);  // only wake() leaves OFF
  t.wake();
  CHECK(t.state() == Backlight::ACTIVE && t.update(10) == SleepAction::NONE);

  // Input while dimmed restores full brightness.
  CHECK(t.update(31000) == SleepAction::DIM);
  CHECK(t.update(5) == SleepAction::RESTORE && t.state() == Backlight::ACTIVE);

  // A long gap (loop stalled) goes straight to off.
  SleepTimer u;
  CHECK(u.update(90000) == SleepAction::TURN_OFF);

  // Manual Screen off.
  SleepTimer v;
  v.force_off();
  CHECK(v.state() == Backlight::OFF && v.update(0) == SleepAction::NONE);

  // Custom times.
  SleepTimer w(10000, 20000);
  CHECK(w.update(10000) == SleepAction::DIM && w.update(20000) == SleepAction::TURN_OFF);

  CHECK(dim_level(100) == 30 && dim_level(20) == 20);

  if (failures == 0)
    std::printf("All sleep tests passed\n");
  return failures == 0 ? 0 : 1;
}
