#pragma once
// Energy over the last 24 h (design ScreenEnergy / BarChart, spec.md "Energy"): kWh used in each
// hour, the same window as HourlyHistory (24 closed hours + the current one). RAM only, pure C++,
// host-tested.
//
// Source: a cumulative kWh counter. Closed hours come from Home Assistant's hourly statistics
// (`change`, contract.md §5.3). The open hour is the live counter minus its value at the start of
// the hour (the `state` of the last closed hour from HA, or the counter seen at the hour rollover).

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace esphome::dial_ui {

class EnergyHistory {
 public:
  static constexpr size_t CLOSED = 24;          // finished hours
  static constexpr size_t POINTS = CLOSED + 1;  // + the current (open) hour
  static constexpr int64_t MIN_VALID_EPOCH = 1704067200;  // 2024-01-01: before this the clock is not set

  // Live value of the cumulative counter (kWh). NaN (unavailable) is kept as "unknown".
  void set_live(double kwh) { this->live_ = kwh; }

  // Rolls the hours forward to `epoch`. When the open hour closes with no gap, the device's own
  // figure (counter now minus counter at the start of that hour) takes its place until Home
  // Assistant's statistics replace it; hours never seen are missing. Returns true on a rollover.
  bool advance(int64_t epoch) {
    if (epoch < MIN_VALID_EPOCH)
      return false;
    int64_t h = epoch / 3600;
    if (this->open_hour_ < 0) {
      this->open_hour_ = h;
      return true;
    }
    if (h <= this->open_hour_)
      return false;  // same hour, or the clock went back: keep what we have
    int64_t steps = h - this->open_hour_;
    this->push_(steps == 1 ? this->open_value_() : NAN);  // a longer gap: live - base spans several hours
    for (int64_t i = 1; i < steps && i <= static_cast<int64_t>(CLOSED); i++)
      this->push_(NAN);
    this->open_hour_ = h;
    this->base_ = this->live_;  // the counter as seen at the rollover (a few seconds late at most)
    return true;
  }

  // Backfill from HA: the closed hour starting at `hour_epoch` used `change` kWh and ended with
  // the counter at `end_state` (NaN if unknown). Ignored outside the 24 closed hours. The last
  // closed hour's end value is also the open hour's starting counter.
  void set_closed(int64_t hour_epoch, double change, double end_state) {
    if (this->open_hour_ < 0 || hour_epoch < 0)
      return;
    int64_t back = this->open_hour_ - hour_epoch / 3600;  // 1 = the last closed hour
    if (back < 1 || back > static_cast<int64_t>(CLOSED))
      return;
    if (!std::isnan(change))
      this->closed_[CLOSED - static_cast<size_t>(back)] = change < 0 ? NAN : static_cast<float>(change);
    if (back == 1 && !std::isnan(end_state))
      this->base_ = end_state;
  }

  // Point i, oldest (0, 24 h ago) to newest (POINTS - 1, the current hour so far): kWh, NaN = missing.
  float point(size_t i) const {
    if (this->open_hour_ < 0 || i >= POINTS)
      return NAN;
    if (i == POINTS - 1)
      return this->open_value_();
    return this->closed_[i];
  }

  // Start of point i's hour (epoch seconds), or -1 before the clock is known.
  int64_t hour_start(size_t i) const {
    if (this->open_hour_ < 0 || i >= POINTS)
      return -1;
    return (this->open_hour_ - static_cast<int64_t>(POINTS - 1 - i)) * 3600;
  }

  bool has_clock() const { return this->open_hour_ >= 0; }

  // Newest point with a value, or -1.
  int latest_index() const {
    for (size_t i = POINTS; i-- > 0;)
      if (!std::isnan(this->point(i)))
        return static_cast<int>(i);
    return -1;
  }

  // Sum of the known points in kWh, NaN with no data.
  float total() const {
    double sum = 0;
    bool any = false;
    for (size_t i = 0; i < POINTS; i++) {
      float v = this->point(i);
      if (!std::isnan(v)) {
        sum += v;
        any = true;
      }
    }
    return any ? static_cast<float>(sum) : NAN;
  }

  double live() const { return this->live_; }
  double base() const { return this->base_; }  // counter at the start of the open hour (NaN = unknown)

 protected:
  float open_value_() const {
    if (std::isnan(this->live_) || std::isnan(this->base_))
      return NAN;
    double v = this->live_ - this->base_;
    return v < 0 ? NAN : static_cast<float>(v);  // a counter that went down was reset or is stale
  }

  void push_(float v) {
    for (size_t i = 0; i + 1 < CLOSED; i++)
      this->closed_[i] = this->closed_[i + 1];
    this->closed_[CLOSED - 1] = v;
  }

  float closed_[CLOSED] = {NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN,
                           NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
  int64_t open_hour_{-1};  // hour number (epoch / 3600) of the open hour; -1 until the clock is set
  double live_{NAN};
  double base_{NAN};
};

// Axis maximum of the bars: the smallest of 1, 1.5, 2, 3, 4, 5, 6, 8 (times a power of ten)
// that holds `v`, never below 1 kWh.
inline float nice_axis_max(float v) {
  if (std::isnan(v) || v <= 1.0f)
    return 1.0f;
  static const float STEPS[] = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 10.0f};
  float base = 1.0f;
  while (base * 10.0f < v)
    base *= 10.0f;
  for (float m : STEPS)
    if (base * m >= v)
      return base * m;
  return base * 10.0f;
}

// Height in px of a bar of `v` on an axis 0..`axis_max` drawn `plot_h` px tall; a real value is
// at least a 2 px bar. 0 when missing (the caller draws a stub).
inline int bar_px(float v, float axis_max, int plot_h) {
  if (std::isnan(v) || !(axis_max > 0))
    return 0;
  if (v < 0)
    v = 0;
  int h = static_cast<int>(std::lround(v / axis_max * plot_h));
  if (h < 2)
    h = 2;
  return h > plot_h ? plot_h : h;
}

// kWh as the page shows it: two decimals below 10 (an hour is often under 1 kWh), one up to 99.9,
// whole kWh above; "--" if missing.
inline void format_kwh(float kwh, char *buf, size_t len) {
  if (std::isnan(kwh))
    std::snprintf(buf, len, "--");
  else if (kwh < 9.995f)
    std::snprintf(buf, len, "%.2f", kwh < 0.005f ? 0.0f : kwh);
  else if (kwh < 99.95f)
    std::snprintf(buf, len, "%.1f", kwh);
  else
    std::snprintf(buf, len, "%.0f", kwh);
}

}  // namespace esphome::dial_ui
