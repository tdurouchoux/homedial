#pragma once
// Rooms and their readings (contract.md §2). Pure C++, host-testable.

#include <cmath>
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
};

struct Room {
  std::string id;    // YAML id, used by set_reading("<room>", "<metric>", x)
  std::string name;  // display name, <= 12 characters (longer is cut with an ellipsis)
  std::vector<Reading> readings;  // 1..3, in Metric order

  int index_of(Metric m) const {
    for (size_t i = 0; i < this->readings.size(); i++)
      if (this->readings[i].metric == m)
        return static_cast<int>(i);
    return -1;
  }
};

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
