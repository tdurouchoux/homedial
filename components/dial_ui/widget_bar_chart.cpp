#include "widget_bar_chart.h"

namespace esphome::dial_ui {

static constexpr int32_t PLOT_H = 72;  // bars stand on the 1 px baseline below
static constexpr int32_t STUB_H = 2;
static constexpr int32_t GAP = 2;      // between bars: 25 bars of 4 px on a 6.5 px pitch (~ 62 %)

void BarChart::create(lv_obj_t *parent, int32_t top) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(box, W, H);
  lv_obj_align(box, LV_ALIGN_TOP_MID, 0, top);

  // Clickable (the default): LVGL records the touched bar.
  lv_obj_t *c = lv_chart_create(box);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(c, W, PLOT_H);
  lv_obj_set_pos(c, 0, 0);
  lv_chart_set_type(c, LV_CHART_TYPE_BAR);
  lv_chart_set_point_count(c, N);
  lv_chart_set_div_line_count(c, 0, 0);
  lv_chart_set_axis_range(c, LV_CHART_AXIS_PRIMARY_Y, 0, PLOT_H);
  // No ground, frame or padding; bars have `radius-sm`-sized tops.
  lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(c, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_column(c, GAP, LV_PART_MAIN);
  lv_obj_set_style_radius(c, 2, LV_PART_ITEMS);

  this->series_ = lv_chart_add_series(c, DIALED_LINE, LV_CHART_AXIS_PRIMARY_Y);
  for (size_t i = 0; i < N; i++) {
    this->px_[i] = STUB_H;
    this->missing_[i] = true;
  }
  lv_chart_set_series_ext_y_array(c, this->series_, this->px_);
  lv_obj_add_flag(c, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
  lv_obj_add_event_cb(c, &BarChart::draw_cb_, LV_EVENT_DRAW_TASK_ADDED, this);
  this->chart_ = c;

  lv_obj_t *base = lv_obj_create(box);
  lv_obj_remove_style_all(base);
  lv_obj_remove_flag(base, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(base, W, 1);
  lv_obj_set_pos(base, 0, PLOT_H);
  lv_obj_set_style_bg_color(base, DIALED_TRACK, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(base, LV_OPA_COVER, LV_PART_MAIN);
}

// Fill colour of each bar (LVGL draws one fill task per point; id2 is the point index).
void BarChart::draw_cb_(lv_event_t *e) {
  auto *self = static_cast<BarChart *>(lv_event_get_user_data(e));
  lv_draw_task_t *task = lv_event_get_draw_task(e);
  auto *base = static_cast<lv_draw_dsc_base_t *>(lv_draw_task_get_draw_dsc(task));
  if (base->part != LV_PART_ITEMS)
    return;
  lv_draw_fill_dsc_t *fill = lv_draw_task_get_fill_dsc(task);
  if (fill == nullptr || base->id2 >= N)
    return;
  size_t i = base->id2;
  if (self->missing_[i])
    fill->color = DIALED_TRACK;
  else if (static_cast<int>(i) == self->highlight_)
    fill->color = DIALED_ACCENT;
  else
    fill->color = DIALED_LINE;
}

void BarChart::set_data(const float *values, float axis_max) {
  bool changed = false;
  for (size_t i = 0; i < N; i++) {
    bool missing = std::isnan(values[i]);
    int32_t h = missing ? STUB_H : bar_px(values[i], axis_max, PLOT_H);
    changed |= h != this->px_[i] || missing != this->missing_[i];
    this->px_[i] = h;
    this->missing_[i] = missing;
  }
  if (changed)
    lv_chart_refresh(this->chart_);
}

void BarChart::highlight(int index) {
  if (index == this->highlight_)
    return;
  this->highlight_ = index;
  lv_obj_invalidate(this->chart_);
}

int BarChart::pressed_index() const {
  uint32_t id = lv_chart_get_pressed_point(this->chart_);
  return id == LV_CHART_POINT_NONE ? -1 : static_cast<int>(id);
}

}  // namespace esphome::dial_ui
