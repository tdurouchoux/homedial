#pragma once
// Shared helpers for the Dialed widgets (design/README.md "Visual foundations").

#include "lvgl.h"
#include "ui_tokens.h"
#include "model_metric.h"

namespace esphome::dial_ui {

// The ring's state colour; OFFLINE uses ink-muted.
inline lv_color_t status_color(Status s) {
  switch (s) {
    case Status::OK:
      return DIALED_OK;
    case Status::WARN:
      return DIALED_WARN;
    case Status::ALERT:
      return DIALED_DANGER;
    default:
      return DIALED_INK_MUTED;
  }
}

// Room page: the value colour alone carries the state (ink when OK).
inline lv_color_t value_color(Status s) { return s == Status::OK ? DIALED_INK : status_color(s); }

// Plain screen: bg colour, not scrollable (a scrollable screen changes how the encoder acts).
inline lv_obj_t *create_screen() {
  lv_obj_t *scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, DIALED_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

// A centred text line whose top edge sits at `top` px (design positions are tops).
inline lv_obj_t *create_text(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, int32_t top,
                             int32_t x_offset = 0) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(l, color, LV_PART_MAIN);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(l, LV_ALIGN_TOP_MID, x_offset, top);
  return l;
}

}  // namespace esphome::dial_ui
