#pragma once
// Adjust (design/components/ScreenAdjust): one value edited on the ring; press or OK saves,
// long press (or swipe down) cancels.

#include "model_settings.h"
#include "widget_common.h"

namespace esphome::dial_ui {

class ScreenAdjust {
 public:
  void create();
  void bind(const AdjustSpec &spec, int value);
  void set_value(int value);
  int value() const { return this->value_; }
  // Value from the ring after a touch drag.
  int value_from_arc() const;
  lv_obj_t *ok_button() const { return this->ok_; }
  lv_obj_t *arc() const { return this->arc_; }
  lv_obj_t *screen() const { return this->screen_; }
  void nudge(int32_t dy) { nudge_in(this->value_label_, 0, dy); }

 protected:
  lv_obj_t *screen_{nullptr};
  lv_obj_t *arc_{nullptr};
  lv_obj_t *name_{nullptr};
  lv_obj_t *value_label_{nullptr};
  lv_obj_t *unit_{nullptr};
  lv_obj_t *ok_{nullptr};
  AdjustSpec spec_{"", 0, 100, 1, 0, ""};
  int value_{0};
};

}  // namespace esphome::dial_ui
