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

static constexpr int64_t T0 = 1767225600;  // 2026-01-01 00:00 UTC, an hour boundary
static int64_t at(int hour, int min = 0, int sec = 0) { return T0 + hour * 3600 + min * 60 + sec; }
static constexpr size_t NOW = EnergyHistory::POINTS - 1;  // index of the open hour

int main() {
  // No clock: everything is unknown.
  EnergyHistory e;
  CHECK(!e.has_clock() && std::isnan(e.point(0)) && std::isnan(e.total()) && e.latest_index() < 0);
  CHECK(!e.advance(1000));
  CHECK(e.hour_start(0) == -1);

  // Boot at 30:30 (hour 30 since T0): the window is hours 6..29 closed + 30 open.
  CHECK(e.advance(at(30, 30)));
  CHECK(e.hour_start(NOW) == at(30) && e.hour_start(0) == at(6));
  for (int h = 6; h < 30; h++)
    e.set_closed(at(h), 0.5, h == 29 ? 1000.0 : NAN);  // HA: 0.5 kWh an hour, counter 1000 at 30:00
  CHECK(near(e.point(0), 0.5f) && near(e.point(NOW - 1), 0.5f));
  CHECK(std::isnan(e.point(NOW)));  // open hour: no live value yet
  CHECK(near(e.total(), 12.0f));
  CHECK(e.latest_index() == static_cast<int>(NOW) - 1);
  // Outside the window, the open hour and the future are ignored.
  e.set_closed(at(5), 9.0, NAN);
  e.set_closed(at(30), 9.0, NAN);
  e.set_closed(at(31), 9.0, NAN);
  CHECK(near(e.total(), 12.0f));
  // A negative change (counter reset in HA) is not a bar.
  e.set_closed(at(7), -3.0, NAN);
  CHECK(std::isnan(e.point(1)));
  e.set_closed(at(7), 0.5, NAN);

  // Live counter: the open hour is the counter minus its value at the start of the hour (1000).
  e.set_live(1000.4);
  CHECK(near(e.point(NOW), 0.4f) && e.latest_index() == static_cast<int>(NOW));
  CHECK(near(e.total(), 12.4f));
  e.set_live(999.0);  // the counter went down: unknown, not negative
  CHECK(std::isnan(e.point(NOW)));
  CHECK(!e.advance(at(30, 59)));  // same hour: nothing changes

  // Rollover to 31:00:05 with no gap: the device's own figure fills hour 30 until HA replaces it.
  e.set_live(1000.9);
  CHECK(e.advance(at(31, 0, 5)));
  CHECK(e.hour_start(NOW) == at(31) && e.hour_start(0) == at(7));
  CHECK(near(e.point(NOW - 1), 0.9f));
  CHECK(near(e.point(NOW), 0.0f));  // base = the counter at the rollover
  CHECK(near(e.point(0), 0.5f));    // hour 6 left the window, hour 7 is now the oldest
  e.set_live(1001.2);
  CHECK(near(e.point(NOW), 0.3f));
  // HA's statistics for hour 30 (0.95 kWh, counter 1000.95 at its end) replace the provisional one.
  e.set_closed(at(30), 0.95, 1000.95);
  CHECK(near(e.point(NOW - 1), 0.95f) && near(e.point(NOW), 0.25f));

  // A gap of several hours: no provisional value, unseen hours missing.
  EnergyHistory g;
  g.advance(at(10, 30));
  g.set_closed(at(9), 0.5, 500.0);
  g.set_live(500.5);
  CHECK(g.advance(at(14, 0, 5)));  // hours 10..13 closed without the device seeing their ends
  for (size_t i = NOW - 4; i < NOW; i++)
    CHECK(std::isnan(g.point(i)));
  CHECK(near(g.point(NOW - 5), 0.5f));  // hour 9 kept, shifted
  CHECK(near(g.point(NOW), 0.0f));
  // A gap longer than the window clears it.
  g.advance(at(80));
  CHECK(std::isnan(g.point(0)) && std::isnan(g.point(NOW - 1)));

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
  format_kwh(0.423f, b, sizeof b);
  CHECK(std::strcmp(b, "0.42") == 0);
  format_kwh(0.0f, b, sizeof b);
  CHECK(std::strcmp(b, "0.00") == 0);
  format_kwh(9.999f, b, sizeof b);
  CHECK(std::strcmp(b, "10.0") == 0);
  format_kwh(12.34f, b, sizeof b);
  CHECK(std::strcmp(b, "12.3") == 0);
  format_kwh(123.4f, b, sizeof b);
  CHECK(std::strcmp(b, "123") == 0);

  if (failures == 0)
    std::printf("test_energy: all passed\n");
  return failures == 0 ? 0 : 1;
}
