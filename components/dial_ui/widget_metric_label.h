#pragma once
// MetricLabel: a small round dot in the metric's colour, then the metric's name, as one centred
// line (Room reading, Sensor and History metric line). `dot` is the diameter in px, chosen per page.

#include "widget_common.h"

namespace esphome::dial_ui {

class MetricLabel {
 public:
  void create(lv_obj_t *parent, const lv_font_t *font, int32_t top, int32_t dot, int32_t x_offset = 0,
              lv_color_t text_color = DIALED_INK_MUTED) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);  // taps reach the parent (Room slot box)
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, dot >= 8 ? 6 : 5, LV_PART_MAIN);
    lv_obj_align(row, LV_ALIGN_TOP_MID, x_offset, top);
    this->dot_ = lv_obj_create(row);
    lv_obj_remove_style_all(this->dot_);
    lv_obj_remove_flag(this->dot_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(this->dot_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(this->dot_, dot, dot);
    lv_obj_set_style_radius(this->dot_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->dot_, LV_OPA_COVER, LV_PART_MAIN);
    this->label_ = lv_label_create(row);
    lv_obj_set_style_text_font(this->label_, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->label_, text_color, LV_PART_MAIN);
    this->row_ = row;
  }

  void set(Metric m, const char *text) {
    lv_obj_set_style_bg_color(this->dot_, metric_color(m), LV_PART_MAIN);
    lv_label_set_text(this->label_, text);
  }

  lv_obj_t *obj() const { return this->row_; }

 protected:
  lv_obj_t *row_{nullptr};
  lv_obj_t *dot_{nullptr};
  lv_obj_t *label_{nullptr};
};

}  // namespace esphome::dial_ui
