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

// The metric's identity colour (the dot beside its name); never a status.
inline lv_color_t metric_color(Metric m) {
  switch (m) {
    case Metric::TEMPERATURE:
      return DIALED_METRIC_TEMP;
    case Metric::HUMIDITY:
      return DIALED_METRIC_HUM;
    default:
      return DIALED_METRIC_AIR;
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

// A hero value that opens the page's child on a tap (Sensor -> History, Electricity -> Energy),
// like a tap on a Room reading: pressed highlight, 12 px extra hit area, the tap bubbles to the
// screen handler.
inline void make_tappable(lv_obj_t *obj) {
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_ext_click_area(obj, DIALED_SPACE_3);
  lv_obj_set_style_radius(obj, DIALED_RADIUS_MD, LV_PART_MAIN);
  lv_obj_set_style_bg_color(obj, DIALED_SURFACE_2, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_STATE_PRESSED);
}

// "Nudge": the object is drawn (dx, dy) px off and eases back to its place. It only moves the
// drawing (translate_x/y), not the layout, so LVGL redraws just the object's box. Used instead
// of full-screen slides (spec.md): content arrives moving the way the finger went.
static constexpr uint32_t NUDGE_MS = 150;

inline void nudge_translate_x_cb(void *obj, int32_t v) {
  lv_obj_set_style_translate_x(static_cast<lv_obj_t *>(obj), v, LV_PART_MAIN);
}
inline void nudge_translate_y_cb(void *obj, int32_t v) {
  lv_obj_set_style_translate_y(static_cast<lv_obj_t *>(obj), v, LV_PART_MAIN);
}

inline void nudge_axis_(lv_obj_t *obj, int32_t d, uint32_t delay_ms, lv_anim_exec_xcb_t cb) {
  lv_anim_delete(obj, cb);
  cb(obj, d);  // first frame already offset; 0 clears a previous offset
  if (d == 0)
    return;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_exec_cb(&a, cb);
  lv_anim_set_values(&a, d, 0);
  lv_anim_set_duration(&a, NUDGE_MS);
  lv_anim_set_delay(&a, delay_ms);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

inline void nudge_in(lv_obj_t *obj, int32_t dx, int32_t dy, uint32_t delay_ms = 0) {
  if (obj == nullptr)
    return;
  nudge_axis_(obj, dx, delay_ms, &nudge_translate_x_cb);
  nudge_axis_(obj, dy, delay_ms, &nudge_translate_y_cb);
}

}  // namespace esphome::dial_ui
