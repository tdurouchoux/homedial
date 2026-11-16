#pragma once
// Sensor page trend line (design ScreenSensor): 96 x 24, the last 24 h of hourly means, `accent`
// 2 px line, missing hours as gaps, the latest point a dot. No gridlines, axes or labels.
// Hidden when fewer than two hours have data (spec.md "Sensor page trend").

#include "model_history.h"
#include "model_room.h"
#include "widget_common.h"

namespace esphome::dial_ui {

class Sparkline {
 public:
  static constexpr int32_t W = 96;
  static constexpr int32_t H = 24;

  void create(lv_obj_t *parent, int32_t top);
  // Draws the reading's history, or hides the line (no history, or not enough data yet).
  void set(const Reading &reading);
  lv_obj_t *obj() const { return this->chart_; }

 protected:
  lv_obj_t *chart_{nullptr};
  lv_chart_series_t *series_{nullptr};
  lv_obj_t *dot_{nullptr};
  int32_t values_[HourlyHistory::POINTS]{};
};

}  // namespace esphome::dial_ui
