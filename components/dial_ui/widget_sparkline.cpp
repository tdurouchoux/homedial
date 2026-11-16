#include "widget_sparkline.h"

namespace esphome::dial_ui {

static constexpr int32_t SCALE = 10;  // chart values are tenths, enough for 1-decimal readings
static constexpr int32_t DOT = 6;

void Sparkline::create(lv_obj_t *parent, int32_t top) {
  lv_obj_t *c = lv_chart_create(parent);
  lv_obj_set_size(c, W, H);
  lv_obj_align(c, LV_ALIGN_TOP_MID, 0, top);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(c, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // the dot on the edge point
  lv_chart_set_type(c, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(c, HourlyHistory::POINTS);
  lv_chart_set_div_line_count(c, 0, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(c, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
  lv_obj_set_style_line_width(c, 2, LV_PART_ITEMS);
  lv_obj_set_style_width(c, 0, LV_PART_INDICATOR);
  lv_obj_set_style_height(c, 0, LV_PART_INDICATOR);

  this->series_ = lv_chart_add_series(c, DIALED_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
  for (auto &v : this->values_)
    v = LV_CHART_POINT_NONE;
  lv_chart_set_series_ext_y_array(c, this->series_, this->values_);
  lv_chart_set_x_start_point(c, this->series_, 0);

  lv_obj_t *d = lv_obj_create(c);
  lv_obj_remove_style_all(d);
  lv_obj_remove_flag(d, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(d, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(d, DOT, DOT);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(d, DIALED_ACCENT, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, LV_PART_MAIN);
  this->dot_ = d;
  this->chart_ = c;
  lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
}

void Sparkline::set(const Reading &reading) {
  if (this->chart_ == nullptr)
    return;
  const Thresholds &t = reading.thresholds;
  float lo = 0, hi = 0;
  if (!reading.has_history || !sparkline_range(reading.history, (t.max - t.min) / 30.0f, &lo, &hi)) {
    lv_obj_add_flag(this->chart_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_chart_set_axis_range(this->chart_, LV_CHART_AXIS_PRIMARY_Y, static_cast<int32_t>(lroundf(lo * SCALE)),
                          static_cast<int32_t>(lroundf(hi * SCALE)));
  for (size_t i = 0; i < HourlyHistory::POINTS; i++) {
    float v = reading.history.point(i);
    this->values_[i] = std::isnan(v) ? LV_CHART_POINT_NONE : static_cast<int32_t>(lroundf(v * SCALE));
  }
  lv_obj_remove_flag(this->chart_, LV_OBJ_FLAG_HIDDEN);
  lv_chart_refresh(this->chart_);
  lv_obj_update_layout(this->chart_);
  int last = reading.history.latest_index();  // >= 0: sparkline_range found data
  lv_point_t p;
  lv_chart_get_point_pos_by_id(this->chart_, this->series_, static_cast<uint32_t>(last), &p);
  lv_obj_set_pos(this->dot_, p.x - DOT / 2, p.y - DOT / 2);
}

}  // namespace esphome::dial_ui
