// Host unit tests for model_history.h.
#include <cmath>
#include <cstdio>

#include "model_history.h"

using namespace esphome::dial_ui;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                 \
    }                                                             \
  } while (0)

static const int64_t T0 = 1763200800;  // 2025-11-15 10:00:00 UTC, an hour boundary
static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main() {
  const size_t NOW = HourlyHistory::POINTS - 1;

  {  // Backfill: closed hours only, inside the window; replaces the device's partial mean.
    HourlyHistory h;
    h.set_closed(T0, 5.0f);  // clock not known yet: ignored
    CHECK(!h.has_data());
    h.add(T0 + 2 * 3600 + 60, 22.0f);   // open hour = T0 + 2 h
    h.set_closed(T0 + 3600, 21.0f);     // last closed hour
    h.set_closed(T0, 20.0f);            // the one before
    h.set_closed(T0 + 2 * 3600, 99.0f); // open hour: ignored
    h.set_closed(T0 - 24 * 3600, 99.0f);  // 26 h back: outside the window
    h.set_closed(T0 - 3600, NAN);       // NaN: ignored
    CHECK(near(h.point(NOW), 22.0f));
    CHECK(near(h.point(NOW - 1), 21.0f));
    CHECK(near(h.point(NOW - 2), 20.0f));
    CHECK(std::isnan(h.point(NOW - 3)));
    h.set_closed(T0 - 22 * 3600, 18.0f);  // 24 h back: the oldest point
    CHECK(near(h.point(0), 18.0f));
  }

  {  // Sparkline range: needs 2 points; padded 15 %; never flatter than min_span.
    HourlyHistory h;
    float lo = 0, hi = 0;
    CHECK(!sparkline_range(h, 1.0f, &lo, &hi));
    h.add(T0, 20.0f);
    CHECK(!sparkline_range(h, 1.0f, &lo, &hi));  // one point only
    h.add(T0 + 3600, 30.0f);
    CHECK(sparkline_range(h, 1.0f, &lo, &hi) && near(lo, 18.5f) && near(hi, 31.5f));
    HourlyHistory f;
    f.add(T0, 21.0f);
    f.add(T0 + 3600, 21.0f);
    CHECK(sparkline_range(f, 1.0f, &lo, &hi) && near(lo, 20.5f) && near(hi, 21.5f));  // flat line
  }

  {  // Empty and unset clock.
    HourlyHistory h;
    CHECK(!h.has_data() && h.latest_index() == -1 && h.hour_start(0) == -1);
    h.add(1000, 20.0f);  // clock not set (epoch near 1970): ignored
    CHECK(!h.has_data());
    h.add(T0, NAN);      // missing value: ignored
    CHECK(!h.has_data());
  }
  {  // Minute samples average into the open hour; rollover closes it.
    HourlyHistory h;
    for (int m = 0; m < 60; m++)
      h.add(T0 + m * 60, m < 30 ? 20.0f : 22.0f);
    CHECK(near(h.point(NOW), 21.0f));
    CHECK(std::isnan(h.point(NOW - 1)));
    h.add(T0 + 3600, 25.0f);  // next hour
    CHECK(near(h.point(NOW - 1), 21.0f) && near(h.point(NOW), 25.0f));
    CHECK(h.hour_start(NOW) == T0 + 3600 && h.hour_start(NOW - 1) == T0);
    CHECK(h.latest_index() == static_cast<int>(NOW));
  }
  {  // Gaps: hours without samples are missing; advance() alone rolls over.
    HourlyHistory h;
    h.add(T0, 20.0f);
    h.advance(T0 + 3 * 3600 + 10);  // 3 hours later, no samples since
    CHECK(near(h.point(NOW - 3), 20.0f));
    CHECK(std::isnan(h.point(NOW - 2)) && std::isnan(h.point(NOW - 1)) && std::isnan(h.point(NOW)));
    CHECK(h.latest_index() == static_cast<int>(NOW - 3));
  }
  {  // More than 24 h away: everything old falls out.
    HourlyHistory h;
    h.add(T0, 20.0f);
    h.add(T0 + 30 * 3600, 18.0f);
    CHECK(near(h.point(NOW), 18.0f));
    for (size_t i = 0; i < NOW; i++)
      CHECK(std::isnan(h.point(i)));
  }
  {  // Exactly 24 closed hours kept, oldest dropped first.
    HourlyHistory h;
    for (int k = 0; k <= 25; k++)
      h.add(T0 + k * 3600, static_cast<float>(k));
    CHECK(near(h.point(NOW), 25.0f) && near(h.point(NOW - 1), 24.0f) && near(h.point(0), 1.0f));
  }
  {  // Clock going back: sample dropped, data kept.
    HourlyHistory h;
    h.add(T0 + 7200, 20.0f);
    h.add(T0, 99.0f);
    CHECK(near(h.point(NOW), 20.0f));
  }
  if (failures == 0)
    std::printf("All history tests passed\n");
  return failures == 0 ? 0 : 1;
}
