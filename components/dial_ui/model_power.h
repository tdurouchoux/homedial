#pragma once
// Live power (design ScreenPower, "Consumption" metric kind). Pure C++, host-tested.

#include <cmath>
#include <cstddef>
#include <cstdio>

#include "model_metric.h"

namespace esphome::dial_ui {

// Fixed thresholds so every consumption screen reads the same (design ScreenPower):
// WARN from 80 % of the contract limit, ALERT from 100 %.
inline Status power_status(float watts, float limit_w, bool online) {
  if (!online || std::isnan(watts) || !(limit_w > 0))
    return Status::OFFLINE;
  float r = watts / limit_w;
  if (r >= 1.0f)
    return Status::ALERT;
  if (r >= 0.8f)
    return Status::WARN;
  return Status::OK;
}

// Ring 0 to the limit, clamped (full at and above the limit). 0 when missing or negative (export).
inline float power_fraction(float watts, float limit_w) {
  if (std::isnan(watts) || !(limit_w > 0) || watts <= 0)
    return 0.0f;
  float f = watts / limit_w;
  return f > 1.0f ? 1.0f : f;
}

// Up to 9999 W in watts (four digits fit value-xl); above, kW with one decimal.
// Writes the number to `value` and "W" / "kW" to `unit`.
inline void format_power(float watts, bool online, char *value, size_t vlen, const char **unit) {
  *unit = "W";
  if (!online || std::isnan(watts)) {
    std::snprintf(value, vlen, "--");
    return;
  }
  float w = std::round(watts);
  if (std::fabs(w) <= 9999.0f) {
    std::snprintf(value, vlen, "%d", static_cast<int>(w));
    return;
  }
  *unit = "kW";
  std::snprintf(value, vlen, "%.1f", watts / 1000.0f);
}

}  // namespace esphome::dial_ui
