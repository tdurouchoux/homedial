#pragma once
// Pure C++ model of room readings: no LVGL, no ESPHome, so it is unit-tested on the host
// (tests/host). Thresholds and ranges come from the dial_ui YAML config (contract.md §3.4).

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace esphome::dial_ui {

// Fixed page order on a room: Temperature, Humidity, Air quality (spec.md; the design's
// CO2 and PM2.5 are replaced by the BME688 BSEC IAQ index, 0 to 500).
enum class Metric : uint8_t { TEMPERATURE = 0, HUMIDITY = 1, AIR_QUALITY = 2 };
constexpr size_t METRIC_COUNT = 3;

enum class Status : uint8_t { OK, WARN, ALERT, OFFLINE };

struct MetricInfo {
  const char *name;        // Sensor page metric line, e.g. "Temperature"
  const char *short_label;  // Room page label (metric + unit), e.g. "Temp °C"
  const char *unit;        // Sensor page unit line, e.g. "°C"
  int decimals;
};

inline const MetricInfo &metric_info(Metric m) {
  static const MetricInfo INFO[METRIC_COUNT] = {
      {"Temperature", "Temp \xC2\xB0" "C", "\xC2\xB0" "C", 1},
      {"Humidity", "Hum %", "%", 0},
      {"Air quality", "Air IAQ", "IAQ", 0},
  };
  return INFO[static_cast<size_t>(m)];
}

// Ring range and status limits. A limit set to NAN is not used.
// ALERT: value < alert_low or value >= alert_high. WARN: value < warn_low or value >= warn_high.
struct Thresholds {
  float min;
  float max;
  float warn_low;
  float alert_low;
  float warn_high;
  float alert_high;
};

// Rounds to the metric's displayed decimals, so the status always matches the number shown
// (IAQ 100.6 is shown "101" and is WARN with warn_high 101).
inline float display_round(float value, Metric m) {
  if (std::isnan(value))
    return value;
  float scale = metric_info(m).decimals == 1 ? 10.0f : 1.0f;
  return std::round(value * scale) / scale;
}

// `value` should already be display_round()ed.
inline Status compute_status(float value, const Thresholds &t, bool online) {
  if (!online || std::isnan(value))
    return Status::OFFLINE;
  if ((!std::isnan(t.alert_low) && value < t.alert_low) || (!std::isnan(t.alert_high) && value >= t.alert_high))
    return Status::ALERT;
  if ((!std::isnan(t.warn_low) && value < t.warn_low) || (!std::isnan(t.warn_high) && value >= t.warn_high))
    return Status::WARN;
  return Status::OK;
}

// Fraction of the ring (0..1) for a value, clamped to the range. 0 when missing.
inline float ring_fraction(float value, const Thresholds &t) {
  if (std::isnan(value) || !(t.max > t.min))
    return 0.0f;
  float f = (value - t.min) / (t.max - t.min);
  return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

// Formats a reading with the metric's decimals; "--" when missing. Returns the length written.
inline int format_value(char *buf, size_t len, float value, Metric m, bool online) {
  if (!online || std::isnan(value))
    return snprintf(buf, len, "--");
  int d = metric_info(m).decimals;
  float half_unit = d == 0 ? 0.5f : 0.05f;
  float v = std::fabs(value) < half_unit ? 0.0f : value;  // never print "-0" or "-0.0"
  int n = snprintf(buf, len, "%.*f", d, v);
  return n;
}

}  // namespace esphome::dial_ui
