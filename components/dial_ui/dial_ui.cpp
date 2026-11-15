#include "dial_ui.h"

#include "esphome/core/log.h"
#include "ui_tokens.h"

namespace esphome::dial_ui {

static const char *const TAG = "dial_ui";

void DialUi::setup() {
  // M1 placeholder screen: bezel track ring + title, from the Dialed tokens.
  this->screen_ = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(this->screen_, DIALED_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(this->screen_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_remove_flag(this->screen_, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *ring = lv_arc_create(this->screen_);
  lv_obj_set_size(ring, DIALED_ARC_SIZE, DIALED_ARC_SIZE);
  lv_obj_center(ring);
  lv_arc_set_rotation(ring, DIALED_ARC_ROTATION);
  lv_arc_set_bg_angles(ring, 0, DIALED_ARC_SWEEP);
  lv_arc_set_range(ring, 0, 100);
  lv_arc_set_value(ring, 0);
  lv_obj_remove_style(ring, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(ring, DIALED_RING_W, LV_PART_MAIN);
  lv_obj_set_style_arc_width(ring, DIALED_RING_W, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(ring, true, LV_PART_MAIN);
  lv_obj_set_style_arc_color(ring, DIALED_TRACK, LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(this->screen_);
  lv_label_set_text(title, "homedial");
  lv_obj_set_style_text_font(title, DIALED_FONT_TITLE, LV_PART_MAIN);
  lv_obj_set_style_text_color(title, DIALED_INK, LV_PART_MAIN);
  lv_obj_center(title);

  lv_screen_load(this->screen_);
}

void DialUi::loop() {}

void DialUi::dump_config() { ESP_LOGCONFIG(TAG, "Dial UI (M1 placeholder screen)"); }

}  // namespace esphome::dial_ui
