#include "widget_page_dots.h"

namespace esphome::dial_ui {

static constexpr int32_t DOT = 8;
static constexpr int32_t BAR_W = 18;
static constexpr uint32_t GLIDE_MS = 150;

void PageDots::create(lv_obj_t *parent) {
  // Absolute layout (no flex): the row is resized and the dots placed by layout_().
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
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

void PageDots::place_(size_t count) {
  if (count > MAX_DOTS)
    count = MAX_DOTS;
  this->count_ = count;
  if (count < 2) {
    lv_obj_add_flag(this->row_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_remove_flag(this->row_, LV_OBJ_FLAG_HIDDEN);
  bool tight = count > 5;
  this->gap_ = tight ? 6 : DIALED_SPACE_2;
  int32_t width = static_cast<int32_t>(count - 1) * (DOT + this->gap_) + BAR_W;
  lv_obj_set_size(this->row_, width, DOT);
  lv_obj_align(this->row_, LV_ALIGN_TOP_MID, 0, tight ? 218 : 222);
  for (size_t i = 0; i < MAX_DOTS; i++) {
    if (i < count)
      lv_obj_remove_flag(this->dots_[i], LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(this->dots_[i], LV_OBJ_FLAG_HIDDEN);
  }
}

// Each dot is 8 px plus up to 10 px of "bar", depending on how close the bar position is to
// it; its colour blends line -> ink the same way. At a whole index this is exactly the design.
void PageDots::layout_(int32_t pos_x256) {
  int32_t x = 0;
  for (size_t i = 0; i < this->count_; i++) {
    int32_t dist = pos_x256 - static_cast<int32_t>(i) * 256;
    if (dist < 0)
      dist = -dist;
    int32_t weight = dist >= 256 ? 0 : 256 - dist;  // 0..256
    int32_t w = DOT + (BAR_W - DOT) * weight / 256;
    lv_obj_t *d = this->dots_[i];
    lv_obj_set_pos(d, x, 0);
    lv_obj_set_size(d, w, DOT);
    lv_obj_set_style_bg_color(d, lv_color_mix(DIALED_INK, DIALED_LINE, weight > 255 ? 255 : weight),
                              LV_PART_MAIN);
    x += w + this->gap_;
  }
}

void PageDots::set(size_t count, size_t active) {
  if (this->row_ == nullptr)
    return;
  lv_anim_delete(this, &PageDots::anim_cb_);
  this->place_(count);
  if (this->count_ >= 2)
    this->layout_(static_cast<int32_t>(active) * 256);
}

void PageDots::anim_cb_(void *self, int32_t v) { static_cast<PageDots *>(self)->layout_(v); }

void PageDots::animate(size_t count, size_t from, size_t active) {
  if (this->row_ == nullptr)
    return;
  this->set(count, from);
  if (this->count_ < 2 || from == active || from >= this->count_ || active >= this->count_) {
    this->set(count, active);
    return;
  }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, this);
  lv_anim_set_exec_cb(&a, &PageDots::anim_cb_);
  lv_anim_set_values(&a, static_cast<int32_t>(from) * 256, static_cast<int32_t>(active) * 256);
  lv_anim_set_duration(&a, GLIDE_MS);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

}  // namespace esphome::dial_ui
