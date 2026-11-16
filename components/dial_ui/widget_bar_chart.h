#pragma once
// BarChart (design/components/BarChart): 24 hourly bars in a 160 x 74 box, an `lv_chart` of type
// BAR. Past hours `line`, the current hour `accent`, hours with no data (not reached or missing)
// 2 px `track` stubs, a 1 px `track` baseline. Bars are coloured one by one in
// LV_EVENT_DRAW_TASK_ADDED (design/lvgl.md). Chart values are bar heights in px, so the stub
// and the minimum bar need no special case in the axis.

#include <cstddef>

#include "widget_common.h"

namespace esphome::dial_ui {

class BarChart {
 public:
  static constexpr size_t N = 24;
  static constexpr int32_t W = DIALED_SAFE_BOX;  // 160
  static constexpr int32_t H = 74;

  void create(lv_obj_t *parent, int32_t top);
  // values[i] in the axis unit (NaN = no data); `current` is the index drawn in `accent`
  // (N = none); the axis runs 0 to `axis_max`.
  void set_data(const float *values, size_t current, float axis_max);

 protected:
  static void draw_cb_(lv_event_t *e);

  lv_obj_t *chart_{nullptr};
  lv_chart_series_t *series_{nullptr};
  int32_t px_[N]{};   // bar heights in px: the chart's values
  uint8_t kind_[N]{};  // 1 line, 2 accent, 3 stub
};

}  // namespace esphome::dial_ui
