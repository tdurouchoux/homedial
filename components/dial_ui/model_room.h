#pragma once
// Rooms and their readings (contract.md §2). Pure C++, host-testable.

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "model_history.h"
#include "model_metric.h"

namespace esphome::dial_ui {

struct Reading {
  Metric metric;
  Thresholds thresholds;
  float value{NAN};
  bool dirty{true};  // set by inbound callbacks, cleared when the views are refreshed
  bool has_history{false};  // YAML `history: true`: sampled once a minute, History page
  HourlyHistory history;
  std::string statistic_id;  // HA entity id for the history backfill (empty: none)
};

struct Room {
  std::string id;    // YAML id, used by set_reading("<room>", "<metric>", x)
  std::string name;  // display name, <= 12 characters (longer is cut with an ellipsis)
  uint8_t icon{0};   // RoomIcon: 0 none, 1..8 (widget_location_label.h order)
  std::vector<Reading> readings;  // 1..3, in Metric order

  int index_of(Metric m) const {
    for (size_t i = 0; i < this->readings.size(); i++)
      if (this->readings[i].metric == m)
        return static_cast<int>(i);
    return -1;
  }
};

// Room page segmented ring (design ScreenRoom): the ring's `total` degrees split into `count` equal
// segments, one per reading in reading order, `gap` degrees apart. Angles are relative to the ring's
// start; integer degrees, the last segment ends exactly at `total`.
struct ArcSpan {
  int32_t start;
  int32_t sweep;
};
inline ArcSpan ring_segment(size_t count, size_t i, int32_t total, int32_t gap) {
  if (count <= 1)
    return {0, total};
  int32_t n = static_cast<int32_t>(count);
  int32_t seg = (total - (n - 1) * gap) / n;
  int32_t start = static_cast<int32_t>(i) * (seg + gap);
  int32_t end = i + 1 == count ? total : start + seg;
  return {start, end - start};
}

// "temperature" | "humidity" | "air_quality" -> Metric; false if unknown.
inline bool parse_metric(const char *s, Metric *out) {
  static const char *const NAMES[METRIC_COUNT] = {"temperature", "humidity", "air_quality"};
  for (size_t i = 0; i < METRIC_COUNT; i++) {
    if (std::string(s) == NAMES[i]) {
      *out = static_cast<Metric>(i);
      return true;
    }
  }
  return false;
}

}  // namespace esphome::dial_ui
