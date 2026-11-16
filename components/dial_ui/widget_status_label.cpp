#include "widget_status_label.h"

namespace esphome::dial_ui {

static constexpr int32_t GLYPH_SIZE = 12;
static constexpr int32_t GLYPH_GAP = 6;
static constexpr int32_t LINE_H = 16;

void StatusLabel::create(lv_obj_t *parent, int32_t top) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LINE_H);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, GLYPH_GAP, LV_PART_MAIN);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, top);

  this->glyph_ = lv_label_create(row);
  lv_obj_set_style_text_font(this->glyph_, DIALED_FONT_CAPTION, LV_PART_MAIN);

  this->ring_ = lv_obj_create(row);
  lv_obj_remove_style_all(this->ring_);
  lv_obj_remove_flag(this->ring_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(this->ring_, GLYPH_SIZE - 2, GLYPH_SIZE - 2);
  lv_obj_set_style_radius(this->ring_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_border_width(this->ring_, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(this->ring_, DIALED_INK_MUTED, LV_PART_MAIN);

  this->word_ = lv_label_create(row);
  lv_obj_set_style_text_font(this->word_, DIALED_FONT_LABEL, LV_PART_MAIN);
  this->row_ = row;
  this->set(Status::OFFLINE);
}

void StatusLabel::set(Status status) {
  if (this->row_ == nullptr)
    return;
  const char *glyph = nullptr;
  const char *word = "OFFLINE";
  switch (status) {
    case Status::OK:
      glyph = LV_SYMBOL_OK;
      word = "OK";
      break;
    case Status::WARN:
      glyph = LV_SYMBOL_WARNING;
      word = "WARN";
      break;
    case Status::ALERT:
      glyph = LV_SYMBOL_CLOSE;
      word = "ALERT";
      break;
    default:
      break;
  }
  lv_color_t c = status_color(status);
  if (glyph != nullptr) {
    lv_label_set_text(this->glyph_, glyph);
    lv_obj_set_style_text_color(this->glyph_, c, LV_PART_MAIN);
    lv_obj_remove_flag(this->glyph_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(this->ring_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(this->glyph_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(this->ring_, LV_OBJ_FLAG_HIDDEN);
  }
  lv_label_set_text(this->word_, word);
  lv_obj_set_style_text_color(this->word_, c, LV_PART_MAIN);
}

}  // namespace esphome::dial_ui
