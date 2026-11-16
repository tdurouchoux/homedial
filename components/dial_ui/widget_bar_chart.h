#pragma once
// BarChart (design/components/BarChart): 24 hourly bars in a 160 x 74 box. Past hours `line`, the
// current hour `accent`, hours with no data (not reached or missing) 2 px `track` stubs, a 1 px
// `track` baseline. Drawn as 24 plain rectangles: each bar has its own colour and height, which
// is simpler and cheaper than an lv_chart with draw-task overrides, and only the bars that
// changed are restyled.

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
  lv_obj_t *box_{nullptr};
  lv_obj_t *bars_[N]{};
  int32_t height_[N]{};   // last drawn height, 0 = not drawn yet
  uint8_t kind_[N]{};     // last drawn colour: 1 line, 2 accent, 3 stub
};

}  // namespace esphome::dial_ui
