#pragma once
// Rooms and their readings (contract.md §2). Pure C++, host-testable.

#include <cmath>
#include <string>
#include <vector>

#include "model_metric.h"

namespace esphome::dial_ui {

struct Reading {
  Metric metric;
  Thresholds thresholds;
  float value{NAN};
  bool dirty{true};  // set by inbound callbacks, cleared when the views are refreshed
};

struct Room {
  std::string id;    // YAML id, used by set_reading("<room>", "<metric>", x)
  std::string name;  // display name, <= 12 characters (longer is cut with an ellipsis)
  std::vector<Reading> readings;  // 1..4, in Metric order

  int index_of(Metric m) const {
    for (size_t i = 0; i < this->readings.size(); i++)
      if (this->readings[i].metric == m)
        return static_cast<int>(i);
    return -1;
  }
};

// "temperature" | "humidity" | "co2" | "pm25" -> Metric; false if unknown.
inline bool parse_metric(const char *s, Metric *out) {
  static const char *const NAMES[METRIC_COUNT] = {"temperature", "humidity", "co2", "pm25"};
  for (size_t i = 0; i < METRIC_COUNT; i++) {
    if (std::string(s) == NAMES[i]) {
      *out = static_cast<Metric>(i);
      return true;
    }
  }
  return false;
}

}  // namespace esphome::dial_ui
