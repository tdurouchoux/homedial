// Host unit tests for the LVGL-free model files of components/dial_ui.
// Run: tests/host/run.sh
#include <cmath>
#include <cstdio>
#include <cstring>

#include "model_metric.h"
#include "model_room.h"

using namespace esphome::dial_ui;

static int failures = 0;
#define CHECK(cond)                                                 \
  do {                                                              \
    if (!(cond)) {                                                  \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
      failures++;                                                   \
    }                                                               \
  } while (0)

// spec.md defaults
static const Thresholds TEMP{10, 40, 18, 16, 26, 30};
static const Thresholds IAQ{0, 500, NAN, NAN, 101, 201};

static void test_status() {
  CHECK(compute_status(21.0f, TEMP, true) == Status::OK);
  CHECK(compute_status(18.0f, TEMP, true) == Status::OK);      // warn_low is exclusive
  CHECK(compute_status(17.9f, TEMP, true) == Status::WARN);
  CHECK(compute_status(15.9f, TEMP, true) == Status::ALERT);
  CHECK(compute_status(25.9f, TEMP, true) == Status::OK);
  CHECK(compute_status(26.0f, TEMP, true) == Status::WARN);    // "warn from 26"
  CHECK(compute_status(30.0f, TEMP, true) == Status::ALERT);   // "alert from 30"
  CHECK(compute_status(NAN, TEMP, true) == Status::OFFLINE);
  CHECK(compute_status(21.0f, TEMP, false) == Status::OFFLINE);
  CHECK(compute_status(0.0f, IAQ, true) == Status::OK);        // no low limits
  CHECK(compute_status(100.0f, IAQ, true) == Status::OK);      // OK up to 100
  CHECK(compute_status(101.0f, IAQ, true) == Status::WARN);
  CHECK(compute_status(200.0f, IAQ, true) == Status::WARN);
  CHECK(compute_status(201.0f, IAQ, true) == Status::ALERT);   // ALERT above 200
  // Status follows the displayed (rounded) value.
  CHECK(compute_status(display_round(100.4f, Metric::AIR_QUALITY), IAQ, true) == Status::OK);
  CHECK(compute_status(display_round(100.6f, Metric::AIR_QUALITY), IAQ, true) == Status::WARN);
  CHECK(compute_status(display_round(25.96f, Metric::TEMPERATURE), TEMP, true) == Status::WARN);  // shown 26.0
  CHECK(compute_status(display_round(25.94f, Metric::TEMPERATURE), TEMP, true) == Status::OK);    // shown 25.9
  CHECK(std::isnan(display_round(NAN, Metric::HUMIDITY)));
}

static void test_ring() {
  CHECK(ring_fraction(10.0f, TEMP) == 0.0f);
  CHECK(ring_fraction(40.0f, TEMP) == 1.0f);
  CHECK(std::fabs(ring_fraction(25.0f, TEMP) - 0.5f) < 1e-6f);
  CHECK(ring_fraction(-5.0f, TEMP) == 0.0f);
  CHECK(ring_fraction(99.0f, TEMP) == 1.0f);
  CHECK(ring_fraction(NAN, TEMP) == 0.0f);
}

static bool fmt_is(float v, Metric m, bool online, const char *want) {
  char buf[16];
  format_value(buf, sizeof(buf), v, m, online);
  if (std::strcmp(buf, want) != 0) {
    std::printf("  format(%g) = '%s', want '%s'\n", v, buf, want);
    return false;
  }
  return true;
}

static void test_format() {
  CHECK(fmt_is(21.84f, Metric::TEMPERATURE, true, "21.8"));
  CHECK(fmt_is(-0.04f, Metric::TEMPERATURE, true, "0.0"));
  CHECK(fmt_is(-3.25f, Metric::TEMPERATURE, true, "-3.2"));
  CHECK(fmt_is(55.6f, Metric::HUMIDITY, true, "56"));
  CHECK(fmt_is(-0.3f, Metric::HUMIDITY, true, "0"));
  CHECK(fmt_is(87.4f, Metric::AIR_QUALITY, true, "87"));
  CHECK(fmt_is(NAN, Metric::AIR_QUALITY, true, "--"));
  CHECK(fmt_is(12.0f, Metric::AIR_QUALITY, false, "--"));
}

static void test_room() {
  Metric m;
  CHECK(parse_metric("air_quality", &m) && m == Metric::AIR_QUALITY);
  CHECK(!parse_metric("co2", &m));
  Room r;
  Reading t;
  t.metric = Metric::TEMPERATURE;
  t.thresholds = TEMP;
  Reading q;
  q.metric = Metric::AIR_QUALITY;
  q.thresholds = IAQ;
  r.readings.push_back(t);
  r.readings.push_back(q);
  CHECK(r.index_of(Metric::AIR_QUALITY) == 1);
  CHECK(r.index_of(Metric::HUMIDITY) == -1);
  CHECK(std::isnan(r.readings[0].value) && r.readings[0].dirty && !r.readings[0].has_history);
  CHECK(!r.readings[0].history.has_data());
}

static void test_ring_segment() {
  ArcSpan a = ring_segment(1, 0, 270, 12);
  CHECK(a.start == 0 && a.sweep == 270);
  a = ring_segment(2, 0, 270, 12);
  ArcSpan b = ring_segment(2, 1, 270, 12);
  CHECK(a.start == 0 && a.sweep == 129 && b.start == 141 && b.start + b.sweep == 270);
  ArcSpan s0 = ring_segment(3, 0, 270, 12), s1 = ring_segment(3, 1, 270, 12), s2 = ring_segment(3, 2, 270, 12);
  CHECK(s0.sweep == 82 && s1.start == 94 && s1.sweep == 82 && s2.start == 188 && s2.start + s2.sweep == 270);
  ArcSpan l = ring_segment(4, 3, 270, 12);  // 58.5 -> 58, the last one takes the remainder
  CHECK(l.start == 210 && l.start + l.sweep == 270);
}

int main() {
  test_status();
  test_ring();
  test_format();
  test_room();
  test_ring_segment();
  if (failures == 0)
    std::printf("All model tests passed\n");
  return failures == 0 ? 0 : 1;
}
