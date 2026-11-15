#include "widget_weather.h"

namespace esphome::dial_ui {

static lv_obj_t *make_ring(lv_obj_t *parent) {
  lv_obj_t *arc = lv_arc_create(parent);
  lv_obj_set_size(arc, DIALED_ARC_SIZE, DIALED_ARC_SIZE);
  lv_obj_center(arc);
  lv_arc_set_rotation(arc, DIALED_ARC_ROTATION);
  lv_arc_set_bg_angles(arc, 0, DIALED_ARC_SWEEP);
  lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc, DIALED_RING_W, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, DIALED_RING_W, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
  lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
  return arc;
}

void RainArc::create(lv_obj_t *parent) {
  this->track_ = make_ring(parent);
  lv_obj_set_style_arc_color(this->track_, DIALED_TRACK, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(this->track_, LV_OPA_TRANSP, LV_PART_INDICATOR);
  for (auto &r : this->runs_) {
    r = make_ring(parent);
    lv_obj_set_style_arc_opa(r, LV_OPA_TRANSP, LV_PART_MAIN);  // segment only: no own track
    lv_obj_set_style_arc_color(r, DIALED_ACCENT, LV_PART_INDICATOR);
  }
}

void RainArc::set(const RainRun *runs, size_t n, bool available) {
  if (this->track_ == nullptr)
    return;
  if (!available)
    n = 0;
  if (!available)
    lv_obj_add_flag(this->track_, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_remove_flag(this->track_, LV_OBJ_FLAG_HIDDEN);
  // 60 min over the 270° sweep: 4.5° per minute, from the lower left, clockwise.
  const float deg_per_min = static_cast<float>(DIALED_ARC_SWEEP) / RainForecast::HORIZON_MIN;
  for (size_t i = 0; i < MAX_RUNS; i++) {
    lv_obj_t *arc = this->runs_[i];
    if (i >= n) {
      lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    int32_t a0 = static_cast<int32_t>(runs[i].from_min * deg_per_min + 0.5f);
    int32_t a1 = static_cast<int32_t>(runs[i].to_min * deg_per_min + 0.5f);
    if (a1 <= a0)
      a1 = a0 + 1;
    lv_arc_set_angles(arc, a0, a1);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_HIDDEN);
  }
}

}  // namespace esphome::dial_ui
