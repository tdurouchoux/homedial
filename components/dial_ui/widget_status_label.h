#pragma once
// StatusLabel (design/components/StatusLabel): glyph + word in the state colour, one 16 px line.

#include "widget_common.h"

namespace esphome::dial_ui {

class StatusLabel {
 public:
  void create(lv_obj_t *parent, int32_t top);
  void set(Status status);

 protected:
  lv_obj_t *row_{nullptr};
  lv_obj_t *glyph_{nullptr};    // LV_SYMBOL_* in caption size
  lv_obj_t *ring_{nullptr};     // OFFLINE glyph: a 12 px ring (stand-in for the dashed ring)
  lv_obj_t *word_{nullptr};
};

}  // namespace esphome::dial_ui
