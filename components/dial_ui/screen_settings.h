#pragma once
// Settings page placeholder (M3). The ListMenu rows (Luminosity, Buzzer vol., Screen off,
// Power off, Exit) arrive in M7; until then this page only proves the navigation.

#include "widget_common.h"

namespace esphome::dial_ui {

class ScreenSettings {
 public:
  void create() {
    this->screen_ = create_screen();
    lv_obj_t *title = create_text(this->screen_, DIALED_FONT_TITLE, DIALED_INK, 0);
    lv_label_set_text(title, "Settings");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -DIALED_SPACE_3);
    lv_obj_t *hint = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK_MUTED, 0);
    lv_label_set_text(hint, "Hold to go back");
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, DIALED_SPACE_5);
  }
  lv_obj_t *screen() const { return this->screen_; }

 protected:
  lv_obj_t *screen_{nullptr};
};

}  // namespace esphome::dial_ui
