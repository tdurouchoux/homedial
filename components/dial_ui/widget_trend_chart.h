#pragma once
// TrendChart (design/components/TrendChart): 160 x 74 line chart of the last 24 h, hourly means.
// Line `accent` 2 px, three `track` gridlines, the last point a dot with a `bg` outline,
// missing hours as gaps, a dashed `warn` line when a limit is crossed in the window.

#include "model_history.h"
#include "model_room.h"
#include "widget_common.h"

namespace esphome::dial_ui {

class TrendChart {
 public:
  static constexpr int32_t W = DIALED_SAFE_BOX;  // 160
  static constexpr int32_t H = 74;

  void create(lv_obj_t *parent, int32_t top);
  void set_data(const HourlyHistory &history, const Reading &reading);
  // Scrub cursor (2 px `ink-muted` line + `accent` dot) on point `index`; -1 hides it.
  void show_cursor(int index);
  // Point nearest to the finger while the chart is pressed, or -1.
  int pressed_index() const;
  lv_obj_t *obj() const { return this->chart_; }

 protected:
  void place_dot_(lv_obj_t *dot, int index);
  int32_t to_chart_(float v) const;

  lv_obj_t *chart_{nullptr};
  lv_chart_series_t *series_{nullptr};
  lv_chart_cursor_t *cursor_{nullptr};
  lv_obj_t *last_dot_{nullptr};
  lv_obj_t *scrub_dot_{nullptr};
  lv_obj_t *limit_line_{nullptr};
  lv_point_precise_t limit_points_[2]{};
  int32_t values_[HourlyHistory::POINTS]{};
  float ymin_{0}, ymax_{1};
  int last_index_{-1};
};

}  // namespace esphome::dial_ui
