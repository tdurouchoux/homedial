#include "screen_settings.h"

namespace esphome::dial_ui {

static constexpr int32_t ROW_W = 176;
static constexpr int32_t ROW_PAD = DIALED_SPACE_4;
// Scroll thumb: a 60° segment travelling along a 120° track on the right of the ring
// (0° = 3 o'clock, clockwise), from the top of the list to its end.
static constexpr int32_t TRACK_START = 300;
static constexpr int32_t TRACK_SPAN = 120;
static constexpr int32_t THUMB_SPAN = 60;

static lv_obj_t *make_thumb_arc(lv_obj_t *parent) {
  lv_obj_t *arc = lv_arc_create(parent);
  lv_obj_set_size(arc, DIALED_ARC_SIZE, DIALED_ARC_SIZE);
  lv_obj_center(arc);
  lv_arc_set_rotation(arc, 0);
  lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc, DIALED_RING_W, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, DIALED_RING_W, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
  return arc;
}

void ScreenSettings::create() {
  this->screen_ = create_screen();
  // Track and thumb.
  this->thumb_ = make_thumb_arc(this->screen_);
  lv_arc_set_bg_angles(this->thumb_, TRACK_START, (TRACK_START + TRACK_SPAN) % 360);
  lv_obj_set_style_arc_color(this->thumb_, DIALED_TRACK, LV_PART_MAIN);
  lv_obj_set_style_arc_color(this->thumb_, DIALED_ACCENT, LV_PART_INDICATOR);

  const int32_t tops[3] = {120 - DIALED_ROW_H / 2 - DIALED_ROW_H, 120 - DIALED_ROW_H / 2,
                           120 + DIALED_ROW_H / 2};
  for (size_t k = 0; k < 3; k++) {
    Slot &s = this->slots_[k];
    s.box = lv_obj_create(this->screen_);
    lv_obj_remove_style_all(s.box);
    lv_obj_remove_flag(s.box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s.box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s.box, LV_OBJ_FLAG_EVENT_BUBBLE);  // taps reach the screen handler
    lv_obj_set_size(s.box, ROW_W, DIALED_ROW_H);
    lv_obj_set_pos(s.box, (DIALED_DISPLAY - ROW_W) / 2, tops[k]);
    lv_obj_set_style_radius(s.box, DIALED_RADIUS_MD, LV_PART_MAIN);
    s.title = lv_label_create(s.box);
    lv_obj_set_style_text_font(s.title, DIALED_FONT_TITLE, LV_PART_MAIN);
    if (k == 1) {
      // Focused row: accent fill, on-accent title and value (design ListMenu).
      lv_obj_set_style_bg_color(s.box, DIALED_ACCENT, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(s.box, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_text_color(s.title, DIALED_ON_ACCENT, LV_PART_MAIN);
      s.value = lv_label_create(s.box);
      lv_obj_set_style_text_font(s.value, DIALED_FONT_LABEL, LV_PART_MAIN);
      lv_obj_set_style_text_color(s.value, DIALED_ON_ACCENT, LV_PART_MAIN);
      lv_obj_align(s.value, LV_ALIGN_RIGHT_MID, -ROW_PAD, 0);
    } else {
      // Neighbours: title in ink at opa-dim, centred, no fill.
      lv_obj_set_style_text_color(s.title, DIALED_INK, LV_PART_MAIN);
      lv_obj_set_style_opa(s.box, DIALED_OPA_DIM, LV_PART_MAIN);
      lv_obj_center(s.title);
    }
  }
  this->draw_();
}

void ScreenSettings::set_values(int luminosity, int volume) {
  this->values_[static_cast<size_t>(SettingsRow::LUMINOSITY)] = luminosity;
  this->values_[static_cast<size_t>(SettingsRow::BUZZER)] = volume;
  this->draw_();
}

void ScreenSettings::set_focus(size_t i, int dir) {
  if (i >= SETTINGS_ROWS)
    i = SETTINGS_ROWS - 1;
  this->focus_ = i;
  this->draw_();
  if (dir != 0)
    for (size_t k = 0; k < 3; k++)
      nudge_in(this->slots_[k].box, 0, dir * 8);  // turning down moves the list up: rows arrive from below
}

void ScreenSettings::draw_() {
  if (this->screen_ == nullptr)
    return;
  for (size_t k = 0; k < 3; k++) {
    Slot &s = this->slots_[k];
    int row = static_cast<int>(this->focus_) + static_cast<int>(k) - 1;
    s.row = (row >= 0 && row < static_cast<int>(SETTINGS_ROWS)) ? row : -1;
    if (s.row < 0) {
      lv_obj_add_flag(s.box, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(s.box, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s.title, settings_label(static_cast<SettingsRow>(s.row)));
    if (k == 1) {
      int v = this->values_[s.row];
      if (v >= 0) {
        lv_label_set_text_fmt(s.value, "%d%%", v);
        lv_obj_remove_flag(s.value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(s.title, LV_ALIGN_LEFT_MID, ROW_PAD, 0);
      } else {
        lv_obj_add_flag(s.value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(s.title);
      }
    }
  }
  int32_t start = TRACK_START + static_cast<int32_t>(this->focus_) * (TRACK_SPAN - THUMB_SPAN) /
                                    static_cast<int32_t>(SETTINGS_ROWS - 1);
  lv_arc_set_angles(this->thumb_, start % 360, (start + THUMB_SPAN) % 360);
}

int ScreenSettings::row_for(lv_obj_t *target) const {
  for (const auto &s : this->slots_)
    if (s.box == target)
      return s.row;
  return -1;
}

}  // namespace esphome::dial_ui
