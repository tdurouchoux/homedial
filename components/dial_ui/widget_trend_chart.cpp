#include "widget_trend_chart.h"

namespace esphome::dial_ui {

static constexpr int32_t SCALE = 10;  // chart values are tenths, enough for 1-decimal readings
static constexpr int32_t DOT = 8;     // "4 px dot" = radius 4

static lv_obj_t *make_dot(lv_obj_t *parent, lv_color_t color) {
  lv_obj_t *d = lv_obj_create(parent);
  lv_obj_remove_style_all(d);
  lv_obj_remove_flag(d, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(d, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(d, DOT, DOT);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(d, color, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_outline_width(d, 2, LV_PART_MAIN);
  lv_obj_set_style_outline_color(d, DIALED_BG, LV_PART_MAIN);
  lv_obj_set_style_outline_opa(d, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
  return d;
}

void TrendChart::create(lv_obj_t *parent, int32_t top) {
  lv_obj_t *c = lv_chart_create(parent);
  lv_obj_set_size(c, W, H);
  lv_obj_align(c, LV_ALIGN_TOP_MID, 0, top);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(c, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // dots on the edge points
  lv_chart_set_type(c, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(c, HourlyHistory::POINTS);
  lv_chart_set_div_line_count(c, 3, 0);
  // Ground and frame: none; gridlines `track` 1 px.
  lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(c, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
  lv_obj_set_style_line_color(c, DIALED_TRACK, LV_PART_MAIN);
  lv_obj_set_style_line_width(c, 1, LV_PART_MAIN);
  // Series line 2 px, no point markers (the last point has its own dot).
  lv_obj_set_style_line_width(c, 2, LV_PART_ITEMS);
  lv_obj_set_style_width(c, 0, LV_PART_INDICATOR);
  lv_obj_set_style_height(c, 0, LV_PART_INDICATOR);
  // Cursor: 2 px line, no point marker (the scrub dot is drawn separately).
  lv_obj_set_style_line_width(c, 2, LV_PART_CURSOR);
  lv_obj_set_style_width(c, 0, LV_PART_CURSOR);

  this->series_ = lv_chart_add_series(c, DIALED_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
  for (auto &v : this->values_)
    v = LV_CHART_POINT_NONE;
  lv_chart_set_series_ext_y_array(c, this->series_, this->values_);
  lv_chart_set_x_start_point(c, this->series_, 0);
  this->cursor_ = lv_chart_add_cursor(c, DIALED_INK_MUTED, LV_DIR_VER);
  lv_chart_set_cursor_point(c, this->cursor_, this->series_, LV_CHART_POINT_NONE);

  this->limit_line_ = lv_line_create(c);
  lv_obj_remove_flag(this->limit_line_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(this->limit_line_, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_style_line_color(this->limit_line_, DIALED_WARN, LV_PART_MAIN);
  lv_obj_set_style_line_width(this->limit_line_, 1, LV_PART_MAIN);
  lv_obj_set_style_line_dash_width(this->limit_line_, 4, LV_PART_MAIN);
  lv_obj_set_style_line_dash_gap(this->limit_line_, 3, LV_PART_MAIN);
  lv_obj_add_flag(this->limit_line_, LV_OBJ_FLAG_HIDDEN);

  this->last_dot_ = make_dot(c, DIALED_ACCENT);
  this->scrub_dot_ = make_dot(c, DIALED_ACCENT);
  this->chart_ = c;
}

int32_t TrendChart::to_chart_(float v) const { return static_cast<int32_t>(lroundf(v * SCALE)); }

void TrendChart::set_data(const HourlyHistory &history, const Reading &reading) {
  const Thresholds &t = reading.thresholds;
  // Axis range: the data, padded; never flatter than 1/30 of the metric's ring range.
  float lo = NAN, hi = NAN;
  bool above = false, below = false;
  for (size_t i = 0; i < HourlyHistory::POINTS; i++) {
    float v = history.point(i);
    if (std::isnan(v))
      continue;
    lo = std::isnan(lo) ? v : (v < lo ? v : lo);
    hi = std::isnan(hi) ? v : (v > hi ? v : hi);
    above |= !std::isnan(t.warn_high) && v >= t.warn_high;
    below |= !std::isnan(t.warn_low) && v < t.warn_low;
  }
  // A limit crossed in the window is drawn as a dashed `warn` line (design TrendChart).
  float limit = above ? t.warn_high : (below ? t.warn_low : NAN);
  if (!std::isnan(limit)) {
    lo = limit < lo ? limit : lo;
    hi = limit > hi ? limit : hi;
  }
  if (std::isnan(lo)) {  // no data: neutral range, empty chart
    lo = t.min;
    hi = t.max;
  }
  float min_span = (t.max - t.min) / 30.0f;
  float pad = (hi - lo) * 0.15f;
  if (hi - lo + 2 * pad < min_span)
    pad = (min_span - (hi - lo)) / 2;
  this->ymin_ = lo - pad;
  this->ymax_ = hi + pad;
  lv_chart_set_axis_range(this->chart_, LV_CHART_AXIS_PRIMARY_Y, this->to_chart_(this->ymin_),
                          this->to_chart_(this->ymax_));

  this->last_index_ = history.latest_index();
  for (size_t i = 0; i < HourlyHistory::POINTS; i++) {
    float v = history.point(i);
    this->values_[i] = std::isnan(v) ? LV_CHART_POINT_NONE : this->to_chart_(v);
  }
  lv_chart_refresh(this->chart_);
  lv_obj_update_layout(this->chart_);

  if (std::isnan(limit)) {
    lv_obj_add_flag(this->limit_line_, LV_OBJ_FLAG_HIDDEN);
  } else {
    int32_t y = static_cast<int32_t>((this->ymax_ - limit) / (this->ymax_ - this->ymin_) * (H - 1) + 0.5f);
    this->limit_points_[0] = {0, 0};
    this->limit_points_[1] = {W - 1, 0};
    lv_line_set_points(this->limit_line_, this->limit_points_, 2);
    lv_obj_set_pos(this->limit_line_, 0, y);
    lv_obj_remove_flag(this->limit_line_, LV_OBJ_FLAG_HIDDEN);
  }
  this->place_dot_(this->last_dot_, this->last_index_);
}

void TrendChart::place_dot_(lv_obj_t *dot, int index) {
  if (index < 0 || this->values_[index] == LV_CHART_POINT_NONE) {
    lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_point_t p;
  lv_chart_get_point_pos_by_id(this->chart_, this->series_, static_cast<uint32_t>(index), &p);
  lv_obj_set_pos(dot, p.x - DOT / 2, p.y - DOT / 2);
  lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
}

void TrendChart::show_cursor(int index) {
  if (index < 0) {
    lv_chart_set_cursor_point(this->chart_, this->cursor_, this->series_, LV_CHART_POINT_NONE);
    lv_obj_add_flag(this->scrub_dot_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(this->chart_);
    return;
  }
  lv_chart_set_cursor_point(this->chart_, this->cursor_, this->series_, static_cast<uint32_t>(index));
  this->place_dot_(this->scrub_dot_, index);
}

int TrendChart::pressed_index() const {
  uint32_t id = lv_chart_get_pressed_point(this->chart_);
  return id == LV_CHART_POINT_NONE ? -1 : static_cast<int>(id);
}

}  // namespace esphome::dial_ui
