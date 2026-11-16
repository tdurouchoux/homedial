#pragma once
// 24 h history of one reading, hourly means, RAM only (contract.md §5, spec.md "Data decisions").
// Pure C++, host-tested. Fed once a minute with the reading's current value from memory
// (not a request to Home Assistant), so every minute weighs the same in the hourly mean.

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace esphome::dial_ui {

class HourlyHistory {
 public:
  static constexpr size_t CLOSED = 24;          // finished hours
  static constexpr size_t POINTS = CLOSED + 1;  // + the current (open) hour
  static constexpr int64_t MIN_VALID_EPOCH = 1704067200;  // 2024-01-01: before this the clock is not set

  // Rolls the hours forward to `epoch` (hours with no sample become missing), then adds the
  // sample to the current hour. NaN values and an unset clock are ignored.
  void add(int64_t epoch, float value) {
    if (epoch < MIN_VALID_EPOCH || std::isnan(value))
      return;
    this->advance(epoch);
    if (epoch / 3600 != this->open_hour_)
      return;  // a sample from an hour already closed (clock went back): dropped
    this->sum_ += value;
    this->count_++;
  }

  // Rolls the hours forward to `epoch` without adding a sample.
  void advance(int64_t epoch) {
    if (epoch < MIN_VALID_EPOCH)
      return;
    int64_t h = epoch / 3600;
    if (this->open_hour_ < 0) {
      this->open_hour_ = h;
      return;
    }
    if (h <= this->open_hour_)
      return;  // same hour, or the clock went back: keep what we have
    int64_t steps = h - this->open_hour_;
    this->push_(this->count_ > 0 ? static_cast<float>(this->sum_ / this->count_) : NAN);
    for (int64_t i = 1; i < steps && i <= static_cast<int64_t>(CLOSED); i++)
      this->push_(NAN);  // hours with no sample at all (device off, HA down)
    this->sum_ = 0;
    this->count_ = 0;
    this->open_hour_ = h;
  }

  // Sets the closed hour that starts at `hour_epoch` to `mean` (backfill from Home Assistant's
  // hourly statistics, contract.md §5.2). HA's mean covers the whole hour, so it replaces the
  // device's own (possibly partial) one. Ignored before the clock is known, for the open hour,
  // outside the 24 h window, and for NaN.
  void set_closed(int64_t hour_epoch, float mean) {
    if (this->open_hour_ < 0 || std::isnan(mean) || hour_epoch < 0)
      return;
    int64_t back = this->open_hour_ - hour_epoch / 3600;  // 1 = the last closed hour
    if (back < 1 || back > static_cast<int64_t>(CLOSED))
      return;
    this->closed_[CLOSED - static_cast<size_t>(back)] = mean;
  }

  // Point i, oldest (0, 24 h ago) to newest (POINTS - 1, the current hour so far). NaN = missing.
  float point(size_t i) const {
    if (i >= POINTS)
      return NAN;
    if (i == POINTS - 1)
      return this->count_ > 0 ? static_cast<float>(this->sum_ / this->count_) : NAN;
    return this->closed_[i];
  }

  // Start of point i's hour (epoch seconds), or -1 before the first sample.
  int64_t hour_start(size_t i) const {
    if (this->open_hour_ < 0 || i >= POINTS)
      return -1;
    return (this->open_hour_ - static_cast<int64_t>(POINTS - 1 - i)) * 3600;
  }

  bool has_data() const {
    for (size_t i = 0; i < POINTS; i++)
      if (!std::isnan(this->point(i)))
        return true;
    return false;
  }

  // Newest point with a value, or -1.
  int latest_index() const {
    for (size_t i = POINTS; i-- > 0;)
      if (!std::isnan(this->point(i)))
        return static_cast<int>(i);
    return -1;
  }

 protected:
  void push_(float v) {
    for (size_t i = 0; i + 1 < CLOSED; i++)
      this->closed_[i] = this->closed_[i + 1];
    this->closed_[CLOSED - 1] = v;
  }

  float closed_[CLOSED] = {NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN,
                           NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
  int64_t open_hour_{-1};
  double sum_{0};
  uint32_t count_{0};
};

// Sensor page sparkline (design ScreenSensor): y range of the history's points, padded 15 % and
// never flatter than `min_span`. False when fewer than 2 points have data (nothing is drawn).
inline bool sparkline_range(const HourlyHistory &h, float min_span, float *lo, float *hi) {
  float a = NAN, b = NAN;
  size_t n = 0;
  for (size_t i = 0; i < HourlyHistory::POINTS; i++) {
    float v = h.point(i);
    if (std::isnan(v))
      continue;
    a = n == 0 ? v : (v < a ? v : a);
    b = n == 0 ? v : (v > b ? v : b);
    n++;
  }
  if (n < 2)
    return false;
  float pad = (b - a) * 0.15f;
  if (b - a + 2 * pad < min_span)
    pad = (min_span - (b - a)) / 2;
  *lo = a - pad;
  *hi = b + pad;
  return true;
}

}  // namespace esphome::dial_ui
