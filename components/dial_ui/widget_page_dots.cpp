#include "widget_page_dots.h"

namespace esphome::dial_ui {

static constexpr int32_t DOT = 8;
static constexpr int32_t BAR_W = 18;

void PageDots::create(lv_obj_t *parent) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(row, LV_SIZE_CONTENT, DOT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  for (auto &d : this->dots_) {
    d = lv_obj_create(row);
    lv_obj_remove_style_all(d);
    lv_obj_remove_flag(d, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  }
  this->row_ = row;
  this->set(0, 0);
}

void PageDots::set(size_t count, size_t active) {
  if (this->row_ == nullptr)
    return;
  if (count < 2) {
    lv_obj_add_flag(this->row_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  if (count > MAX_DOTS)
    count = MAX_DOTS;
  lv_obj_remove_flag(this->row_, LV_OBJ_FLAG_HIDDEN);
  bool tight = count > 5;
  lv_obj_set_style_pad_column(this->row_, tight ? 6 : DIALED_SPACE_2, LV_PART_MAIN);
  lv_obj_align(this->row_, LV_ALIGN_TOP_MID, 0, tight ? 218 : 222);
  for (size_t i = 0; i < MAX_DOTS; i++) {
    lv_obj_t *d = this->dots_[i];
    if (i >= count) {
      lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(d, LV_OBJ_FLAG_HIDDEN);
    bool on = i == active;
    lv_obj_set_size(d, on ? BAR_W : DOT, DOT);
    lv_obj_set_style_bg_color(d, on ? DIALED_INK : DIALED_LINE, LV_PART_MAIN);
  }
}

}  // namespace esphome::dial_ui
