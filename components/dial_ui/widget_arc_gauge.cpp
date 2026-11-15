#include "widget_arc_gauge.h"

namespace esphome::dial_ui {

static constexpr int32_t ARC_STEPS = 1000;
static constexpr uint32_t ARC_ANIM_MS = 250;

void ArcGauge::create(lv_obj_t *parent) {
  lv_obj_t *arc = lv_arc_create(parent);
  lv_obj_set_size(arc, DIALED_ARC_SIZE, DIALED_ARC_SIZE);
  lv_obj_center(arc);
  lv_arc_set_rotation(arc, DIALED_ARC_ROTATION);
  lv_arc_set_bg_angles(arc, 0, DIALED_ARC_SWEEP);
  lv_arc_set_range(arc, 0, ARC_STEPS);
  lv_arc_set_value(arc, 0);
  lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc, DIALED_RING_W, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, DIALED_RING_W, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, DIALED_TRACK, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, DIALED_OK, LV_PART_INDICATOR);
  this->arc_ = arc;
}

void ArcGauge::anim_cb_(void *arc, int32_t v) { lv_arc_set_value(static_cast<lv_obj_t *>(arc), v); }

void ArcGauge::set(float fraction, Status status, bool animate) {
  if (this->arc_ == nullptr)
    return;
  // OFFLINE: empty indicator (design ArcGauge).
  int32_t target = status == Status::OFFLINE ? 0 : static_cast<int32_t>(fraction * ARC_STEPS + 0.5f);
  lv_obj_set_style_arc_color(this->arc_, status_color(status), LV_PART_INDICATOR);
  lv_anim_delete(this->arc_, &ArcGauge::anim_cb_);
  int32_t current = lv_arc_get_value(this->arc_);
  if (!animate || current == target) {
    lv_arc_set_value(this->arc_, target);
    return;
  }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, this->arc_);
  lv_anim_set_exec_cb(&a, &ArcGauge::anim_cb_);
  lv_anim_set_values(&a, current, target);
  lv_anim_set_duration(&a, ARC_ANIM_MS);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

}  // namespace esphome::dial_ui
