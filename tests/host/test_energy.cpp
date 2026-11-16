// Host unit tests for model_energy.h.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "model_energy.h"

using namespace esphome::dial_ui;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                 \
    }                                                             \
  } while (0)
static bool near(float a, float b) { return std::fabs(a - b) < 0.005f; }

// Local time = UTC+1, no DST (enough for the model: it only needs day and hour).
static LocalTime utc1(int64_t e) {
  int64_t l = e + 3600;
  return LocalTime{l / 86400, static_cast<int>((l % 86400) / 3600)};
}

static constexpr int64_t DAY0 = 1767225600;  // 2026-01-01 00:00 UTC
// Epoch of local `hour`:`min` on local day `d` (0 = 2026-01-01 local, UTC+1).
static int64_t at(int d, int hour, int min = 0, int sec = 0) {
  return DAY0 - 3600 + d * 86400 + hour * 3600 + min * 60 + sec;
}

static EnergyDay make() {
  EnergyDay e;
  e.set_local_time(&utc1);
  return e;
}

int main() {
  // Civil dates.
  CHECK(days_from_civil(1970, 1, 1) == 0);
  CHECK(days_from_civil(2026, 1, 1) == DAY0 / 86400);
  CHECK(days_from_civil(2026, 3, 1) - days_from_civil(2026, 2, 28) == 1);

  // No clock: everything is unknown.
  EnergyDay e = make();
  CHECK(!e.has_clock());
  CHECK(std::isnan(e.bar(0)) && std::isnan(e.today_total()));
  CHECK(!e.advance(1000));  // clock not set
  CHECK(!e.has_clock());

  // Boot at 10:30 on day 1; today's hours 0-9 known.
  CHECK(e.advance(at(1, 10, 30)));
  CHECK(e.has_clock() && e.current_hour() == 10);
  for (int h = 0; h < 10; h++)
    e.set_closed(at(1, h), 0.5, h == 9 ? 1000.0 : NAN);  // today 0.5 kWh an hour
  CHECK(near(e.bar(0), 0.5f) && near(e.bar(9), 0.5f));
  CHECK(std::isnan(e.bar(10)));  // open hour: no live value yet
  CHECK(std::isnan(e.bar(11)));  // not reached
  CHECK(near(e.today_total(), 5.0f));

  // Live counter: the open hour is the counter minus its value at the start of the hour (1000).
  e.set_live(1000.4);
  CHECK(near(e.bar(10), 0.4f));
  CHECK(near(e.today_total(), 5.4f));
  e.set_live(999.0);  // the counter went down: unknown, not negative
  CHECK(std::isnan(e.bar(10)));

  // Same hour again: nothing changes.
  CHECK(!e.advance(at(1, 10, 59)));

  // Rollover to 11:00:05 with no gap: the device's own figure fills hour 10 until HA replaces it.
  e.set_live(1000.9);
  CHECK(e.advance(at(1, 11, 0, 5)));
  CHECK(e.current_hour() == 11);
  CHECK(near(e.bar(10), 0.9f));
  CHECK(near(e.bar(11), 0.0f));  // base = the counter at the rollover
  e.set_live(1001.2);
  CHECK(near(e.bar(11), 0.3f));
  // HA's statistics for hour 10 (0.95 kWh, counter 1000.95 at its end) replace the provisional one.
  e.set_closed(at(1, 10), 0.95, 1000.95);
  CHECK(near(e.bar(10), 0.95f));
  CHECK(near(e.bar(11), 0.25f));
  // The open hour and the future are never set by backfill.
  e.set_closed(at(1, 11), 9.0, NAN);
  e.set_closed(at(1, 12), 9.0, NAN);
  CHECK(near(e.bar(11), 0.25f) && std::isnan(e.bar(12)));
  // Hours of earlier days are dropped.
  e.set_closed(at(0, 5), 9.0, NAN);
  CHECK(near(e.today_total(), 5.0f + 0.95f + 0.25f));

  // A gap of several hours: no provisional value, the open hour starts again at the next counter.
  EnergyDay g = make();
  g.advance(at(1, 10, 30));
  g.set_closed(at(1, 9), 0.5, 500.0);
  g.set_live(500.5);
  g.advance(at(1, 14, 0, 5));  // 3+ hours later
  CHECK(std::isnan(g.bar(10)));  // not the 4-hour difference
  CHECK(g.current_hour() == 14);

  // Day change: today starts empty; the last hour of the old day only gives the counter base.
  EnergyDay d = make();
  d.advance(at(1, 23, 30));
  d.set_closed(at(1, 22), 1.0, 1999.0);  // base for hour 23
  d.set_live(2000.5);
  CHECK(near(d.bar(23), 1.5f));
  CHECK(d.advance(at(2, 0, 0, 10)));
  CHECK(d.current_hour() == 0);
  CHECK(std::isnan(d.bar(5)));  // not reached
  CHECK(near(d.bar(0), 0.0f) && near(d.today_total(), 0.0f));  // the open hour starts at the rollover counter
  d.set_closed(at(1, 23), 1.1, 2000.6);  // yesterday's hour is not kept, its end counter is the base
  CHECK(std::isnan(d.bar(23)));
  d.set_live(2000.9);
  CHECK(near(d.bar(0), 0.3f));
  // After a gap of more than a day nothing is known either.
  d.advance(at(5, 8, 0, 5));
  CHECK(std::isnan(d.bar(0)) && near(d.today_total(), 0.0f));  // only the open hour is known

  // Axis maximum.
  CHECK(nice_axis_max(NAN) == 1.0f && nice_axis_max(0.2f) == 1.0f && nice_axis_max(1.0f) == 1.0f);
  CHECK(nice_axis_max(1.2f) == 1.5f && nice_axis_max(2.4f) == 3.0f && nice_axis_max(7.0f) == 8.0f);
  CHECK(nice_axis_max(8.5f) == 10.0f && nice_axis_max(11.0f) == 15.0f && nice_axis_max(95.0f) == 100.0f);

  // Bar heights.
  CHECK(bar_px(NAN, 2.0f, 70) == 0);
  CHECK(bar_px(0.0f, 2.0f, 70) == 2);   // a real zero is still a 2 px bar
  CHECK(bar_px(1.0f, 2.0f, 70) == 35);
  CHECK(bar_px(5.0f, 2.0f, 70) == 70);  // clamped to the plot

  // Text.
  char b[24];
  format_kwh(NAN, b, sizeof b);
  CHECK(std::strcmp(b, "--") == 0);
  format_kwh(12.34f, b, sizeof b);
  CHECK(std::strcmp(b, "12.3") == 0);
  format_kwh(0.0f, b, sizeof b);
  CHECK(std::strcmp(b, "0.0") == 0);
  format_kwh(123.4f, b, sizeof b);
  CHECK(std::strcmp(b, "123") == 0);

  if (failures == 0)
    std::printf("test_energy: all passed\n");
  return failures == 0 ? 0 : 1;
}
