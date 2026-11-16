#pragma once
// Energy today (design ScreenEnergy / BarChart, spec.md "Energy"): 24 hourly kWh bars for the
// local calendar day, plus yesterday's hours for the delta. RAM only, pure C++, host-tested.
//
// Source: a cumulative kWh counter. Closed hours come from Home Assistant's hourly statistics
// (`change`, contract.md §5.3). The open hour is the live counter minus its value at the start
// of the hour (the `state` of the last closed hour, or the counter seen at the hour rollover).

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace esphome::dial_ui {

// Local calendar day (any number that grows by one per day) and hour 0-23 of an epoch.
struct LocalTime {
  int64_t day{0};
  int hour{0};
};
using LocalTimeFn = LocalTime (*)(int64_t epoch);

// Days since 1970-01-01 of a civil date (Howard Hinnant's days_from_civil), for LocalTime::day.
inline int64_t days_from_civil(int y, int m, int d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  int64_t yoe = y - era * 400;
  int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

class EnergyDay {
 public:
  static constexpr size_t HOURS = 24;
  static constexpr int64_t MIN_VALID_EPOCH = 1704067200;  // 2024-01-01: before this the clock is not set

  void set_local_time(LocalTimeFn fn) { this->local_ = fn; }

  // Live value of the cumulative counter (kWh). NaN (unavailable) is kept as "unknown".
  void set_live(double kwh) { this->live_ = kwh; }

  // Rolls the day and hour forward to `epoch`. When an hour closes with no gap, the device's own
  // figure (counter now minus counter at the start of that hour) takes its place until Home
  // Assistant's statistics replace it. Returns true when the hour or the day changed.
  bool advance(int64_t epoch) {
    if (this->local_ == nullptr || epoch < MIN_VALID_EPOCH)
      return false;
    int64_t h = epoch / 3600;
    LocalTime lt = this->local_(epoch);
    if (this->open_hour_ < 0) {
      this->open_hour_ = h;
      this->day_ = lt.day;
      this->cur_hour_ = lt.hour;
      return true;
    }
    if (h <= this->open_hour_)
      return false;  // same hour, or the clock went back: keep what we have
    if (h == this->open_hour_ + 1) {
      float v = this->open_value_();
      if (!std::isnan(v))
        this->store_(this->day_, this->cur_hour_, v);
    }
    if (lt.day != this->day_) {
      if (lt.day == this->day_ + 1) {
        for (size_t i = 0; i < HOURS; i++) {
          this->yday_[i] = this->today_[i];
          this->today_[i] = NAN;
        }
      } else {  // a gap of more than a day: nothing known
        for (size_t i = 0; i < HOURS; i++)
          this->yday_[i] = this->today_[i] = NAN;
      }
      this->day_ = lt.day;
    }
    this->cur_hour_ = lt.hour;
    this->open_hour_ = h;
    this->base_ = this->live_;  // the counter as seen at the rollover (a few seconds late at most)
    return true;
  }

  // Backfill from HA: the closed hour starting at `hour_epoch` used `change` kWh and ended with
  // the counter at `end_state` (NaN if unknown). Only hours of today or yesterday that are closed
  // are kept. The last closed hour's end value is also the open hour's starting counter.
  void set_closed(int64_t hour_epoch, double change, double end_state) {
    if (this->local_ == nullptr || this->open_hour_ < 0 || std::isnan(change) || hour_epoch < 0)
      return;
    int64_t h = hour_epoch / 3600;
    if (h >= this->open_hour_)
      return;
    LocalTime lt = this->local_(hour_epoch);
    this->store_(lt.day, lt.hour, static_cast<float>(change));
    if (h == this->open_hour_ - 1 && !std::isnan(end_state))
      this->base_ = end_state;
  }

  bool has_clock() const { return this->open_hour_ >= 0; }
  size_t current_hour() const { return static_cast<size_t>(this->cur_hour_); }

  // Bar i (local hour i of today): kWh, or NaN when not reached or unknown.
  float bar(size_t i) const {
    if (this->open_hour_ < 0 || i >= HOURS || static_cast<int>(i) > this->cur_hour_)
      return NAN;
    if (static_cast<int>(i) == this->cur_hour_)
      return this->open_value_();
    return this->today_[i];
  }

  // Today so far in kWh (sum of the known hours), NaN with no data.
  float today_total() const {
    double sum = 0;
    bool any = false;
    for (size_t i = 0; i < HOURS; i++) {
      float v = this->bar(i);
      if (!std::isnan(v)) {
        sum += v;
        any = true;
      }
    }
    return any ? static_cast<float>(sum) : NAN;
  }

  // Change in percent of today's closed hours against the same hours yesterday. NaN when there
  // is no closed hour yet, a hour is missing on either day, or yesterday used no energy.
  float delta_percent() const {
    if (this->open_hour_ < 0 || this->cur_hour_ < 1)
      return NAN;
    double t = 0, y = 0;
    for (int i = 0; i < this->cur_hour_; i++) {
      float a = this->today_[i], b = this->yday_[i];
      if (std::isnan(a) || std::isnan(b))
        return NAN;
      t += a;
      y += b;
    }
    if (!(y > 0.05))
      return NAN;
    return static_cast<float>((t - y) / y * 100.0);
  }

 protected:
  float open_value_() const {
    if (std::isnan(this->live_) || std::isnan(this->base_))
      return NAN;
    double v = this->live_ - this->base_;
    return v < 0 ? NAN : static_cast<float>(v);  // a counter that went down was reset or is stale
  }

  void store_(int64_t day, int hour, float v) {
    if (hour < 0 || hour >= static_cast<int>(HOURS))
      return;
    if (day == this->day_)
      this->today_[hour] = v;
    else if (day == this->day_ - 1)
      this->yday_[hour] = v;
  }

  LocalTimeFn local_{nullptr};
  float today_[HOURS] = {NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN,
                         NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
  float yday_[HOURS] = {NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN,
                        NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
  int64_t open_hour_{-1};  // UTC hour number of the open hour; -1 until the clock is set
  int64_t day_{0};
  int cur_hour_{0};
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

// Today's total as the page shows it: one decimal up to 99.9, whole kWh above; "--" if missing.
inline void format_kwh(float kwh, char *buf, size_t len) {
  if (std::isnan(kwh))
    std::snprintf(buf, len, "--");
  else if (kwh < 99.95f)
    std::snprintf(buf, len, "%.1f", kwh < 0.05f ? 0.0f : kwh);
  else
    std::snprintf(buf, len, "%.0f", kwh);
}

// "+12% vs yday", "-3% vs yday" or "0% vs yday" (clamped to +-999). Returns the sign of the
// rounded figure (-1, 0, 1) so the page can pick the arrow, or 0 with an empty text when missing.
inline int format_delta(float pct, char *buf, size_t len) {
  if (std::isnan(pct)) {
    std::snprintf(buf, len, "%s", "");
    return 0;
  }
  if (pct > 999.0f)
    pct = 999.0f;
  if (pct < -999.0f)
    pct = -999.0f;
  int r = static_cast<int>(std::lround(pct));
  if (r == 0)
    std::snprintf(buf, len, "0%% vs yday");
  else
    std::snprintf(buf, len, "%+d%% vs yday", r);
  return r > 0 ? 1 : (r < 0 ? -1 : 0);
}

}  // namespace esphome::dial_ui
