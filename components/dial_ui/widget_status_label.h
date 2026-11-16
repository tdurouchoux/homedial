#pragma once
// StatusLabel (design/components/StatusLabel): glyph + word in the state colour, one 16 px line.

#include "widget_common.h"

namespace esphome::dial_ui {

class StatusLabel {
 public:
  void create(lv_obj_t *parent, int32_t top);
  void set(Status status);
  void set_visible(bool visible) {
    if (this->row_ == nullptr)
      return;
    if (visible)
      lv_obj_remove_flag(this->row_, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(this->row_, LV_OBJ_FLAG_HIDDEN);
  }

 protected:
  lv_obj_t *row_{nullptr};
  lv_obj_t *glyph_{nullptr};    // LV_SYMBOL_* in caption size
  lv_obj_t *ring_{nullptr};     // OFFLINE glyph: a 12 px ring (stand-in for the dashed ring)
  lv_obj_t *word_{nullptr};
};

}  // namespace esphome::dial_ui
