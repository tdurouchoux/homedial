#include "screen_adjust.h"

namespace esphome::dial_ui {

// Tops as on the Sensor page; OK button at top 176 (design ScreenAdjust).
static constexpr int32_t NAME_TOP = 63;
static constexpr int32_t VALUE_TOP = 86;
static constexpr int32_t UNIT_TOP = 142;
static constexpr int32_t OK_TOP = 176;
static constexpr int32_t KNOB = 20;

void ScreenAdjust::create() {
  this->screen_ = create_screen();
  // The whole ring is the slider: accent indicator, ink knob outlined in bg.
  lv_obj_t *arc = lv_arc_create(this->screen_);
  lv_obj_set_size(arc, DIALED_ARC_SIZE, DIALED_ARC_SIZE);
  lv_obj_center(arc);
  lv_arc_set_rotation(arc, DIALED_ARC_ROTATION);
  lv_arc_set_bg_angles(arc, 0, DIALED_ARC_SWEEP);
  lv_obj_set_style_arc_width(arc, DIALED_RING_W, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, DIALED_RING_W, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, DIALED_TRACK, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, DIALED_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(arc, DIALED_INK, LV_PART_KNOB);
  lv_obj_set_style_bg_opa(arc, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_pad_all(arc, (KNOB - DIALED_RING_W) / 2, LV_PART_KNOB);
  lv_obj_set_style_outline_width(arc, 2, LV_PART_KNOB);
  lv_obj_set_style_outline_color(arc, DIALED_BG, LV_PART_KNOB);
  lv_obj_set_style_outline_opa(arc, LV_OPA_COVER, LV_PART_KNOB);
  // A drag on the ring edits the value; it must not turn into a swipe.
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_GESTURE_BUBBLE);
  this->arc_ = arc;

  this->name_ = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK_MUTED, NAME_TOP);
  this->value_label_ = create_text(this->screen_, DIALED_FONT_VALUE_XL, DIALED_INK, VALUE_TOP);
  this->unit_ = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK_MUTED, UNIT_TOP);

  lv_obj_t *ok = lv_obj_create(this->screen_);
  lv_obj_remove_style_all(ok);
  lv_obj_remove_flag(ok, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ok, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ok, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_size(ok, 84, 32);
  lv_obj_align(ok, LV_ALIGN_TOP_MID, 0, OK_TOP);
  lv_obj_set_ext_click_area(ok, 6);  // touch area 44 px high
  lv_obj_set_style_radius(ok, DIALED_RADIUS_SM, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ok, DIALED_ACCENT, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ok, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_opa(ok, DIALED_OPA_PRESSED, LV_STATE_PRESSED);
  lv_obj_t *ok_label = lv_label_create(ok);
  lv_label_set_text(ok_label, "OK");
  lv_obj_set_style_text_font(ok_label, DIALED_FONT_LABEL, LV_PART_MAIN);
  lv_obj_set_style_text_color(ok_label, DIALED_ON_ACCENT, LV_PART_MAIN);
  lv_obj_center(ok_label);
  this->ok_ = ok;
}

void ScreenAdjust::bind(const AdjustSpec &spec, int value) {
  this->spec_ = spec;
  lv_label_set_text(this->name_, spec.name);
  lv_label_set_text(this->unit_, spec.unit);
  lv_arc_set_range(this->arc_, 0, static_cast<int32_t>(setting_positions(spec)) - 1);
  this->set_value(value);
}

void ScreenAdjust::set_value(int value) {
  this->value_ = clamp_setting(this->spec_, value);
  lv_arc_set_value(this->arc_, static_cast<int32_t>(setting_index(this->spec_, this->value_)));
  lv_label_set_text_fmt(this->value_label_, "%d", this->value_);
}

int ScreenAdjust::value_from_arc() const {
  return setting_at(this->spec_, static_cast<size_t>(lv_arc_get_value(this->arc_)));
}

}  // namespace esphome::dial_ui
