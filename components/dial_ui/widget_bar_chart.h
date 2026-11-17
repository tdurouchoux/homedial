#pragma once
// BarChart (design/components/BarChart): one bar per hour in a 160 x 74 box, an `lv_chart` of type
// BAR. Bars `line`, the highlighted one (the current hour, or the scrubbed hour) `accent`, hours
// with no data 2 px `track` stubs, a 1 px `track` baseline. Bars are coloured one by one in
// LV_EVENT_DRAW_TASK_ADDED (design/lvgl.md). Chart values are bar heights in px, so the stub
// and the minimum bar need no special case in the axis.

#include <cstddef>

#include "model_energy.h"
#include "widget_common.h"

namespace esphome::dial_ui {

class BarChart {
 public:
  static constexpr size_t N = EnergyHistory::POINTS;  // 25: -24 h to now, like TrendChart
  static constexpr int32_t W = DIALED_SAFE_BOX;      // 160
  static constexpr int32_t H = 74;

  void create(lv_obj_t *parent, int32_t top);
  // values[i] in the axis unit (NaN = no data), the axis runs 0 to `axis_max`.
  void set_data(const float *values, float axis_max);
  // The bar drawn in `accent`: the current hour when live, the scrubbed hour while scrubbing.
  // -1: none.
  void highlight(int index);
  // Bar touched (recorded by LVGL on LV_EVENT_PRESSED), or -1.
  int pressed_index() const;
  lv_obj_t *obj() const { return this->chart_; }

 protected:
  static void draw_cb_(lv_event_t *e);

  lv_obj_t *chart_{nullptr};
  lv_chart_series_t *series_{nullptr};
  int32_t px_[N]{};     // bar heights in px: the chart's values
  bool missing_[N]{};   // drawn as a `track` stub
  int highlight_{-1};
};

}  // namespace esphome::dial_ui
