#pragma once
// Settings list (design/components/ListMenu; spec.md "Settings list"): three rows visible,
// the focused one in the middle (accent fill), a scroll thumb on the right of the ring.

#include "model_settings.h"
#include "widget_common.h"

namespace esphome::dial_ui {

class ScreenSettings {
 public:
  void create();
  void set_values(int luminosity, int volume);
  // Focus row `i`; dir (-1 / +1 / 0) nudges the rows the way the list moved.
  void set_focus(size_t i, int dir = 0);
  size_t focus() const { return this->focus_; }
  // Row index for a tapped row box, or -1.
  int row_for(lv_obj_t *target) const;
  void nudge(int32_t dy) {
    for (size_t k = 0; k < 3; k++)
      nudge_in(this->slots_[k].box, 0, dy, static_cast<uint32_t>(k) * 30);
  }
  lv_obj_t *screen() const { return this->screen_; }

 protected:
  struct Slot {
    lv_obj_t *box{nullptr};
    lv_obj_t *title{nullptr};
    lv_obj_t *value{nullptr};  // focused slot only
    int row{-1};
  };
  void draw_();

  lv_obj_t *screen_{nullptr};
  lv_obj_t *thumb_{nullptr};
  Slot slots_[3];  // previous, focused, next
  size_t focus_{0};
  int values_[SETTINGS_ROWS]{-1, -1, -1, -1, -1};
};

}  // namespace esphome::dial_ui
