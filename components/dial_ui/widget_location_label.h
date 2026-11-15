#pragma once
// LocationLabel (design/components/LocationLabel): room name, one centred 120 px line, ellipsis.
// RoomIcon is not drawn yet (icon font comes with the weather icons).

#include "widget_common.h"

namespace esphome::dial_ui {

static constexpr int32_t LOCATION_LABEL_W = 120;

inline lv_obj_t *create_location_label(lv_obj_t *parent, const lv_font_t *font, int32_t top) {
  lv_obj_t *l = create_text(parent, font, DIALED_INK, top);
  lv_obj_set_width(l, LOCATION_LABEL_W);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  return l;
}

}  // namespace esphome::dial_ui
