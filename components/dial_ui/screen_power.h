#pragma once
// Electricity (design/components/ScreenPower): live power on the ring gauge, 0 to the contract limit.

#include "model_power.h"
#include "widget_arc_gauge.h"
#include "widget_common.h"
#include "widget_page_dots.h"
#include "widget_status_label.h"

namespace esphome::dial_ui {

class ScreenPower {
 public:
  void create(float limit_w);
  void update(float watts, bool online, bool animate);
  void set_dots(size_t count, size_t active) { this->dots_.set(count, active); }
  void animate_dots(size_t count, size_t from, size_t active) { this->dots_.animate(count, from, active); }
  void nudge(int32_t dx, int32_t dy) {
    nudge_in(this->value_, dx, dy);
    nudge_in(this->unit_, dx, 0, 30);
  }
  void reset_ring() { this->gauge_.reset(); }
  lv_obj_t *screen() const { return this->screen_; }

 protected:
  lv_obj_t *screen_{nullptr};
  lv_obj_t *value_{nullptr};
  lv_obj_t *unit_{nullptr};
  ArcGauge gauge_;
  StatusLabel status_;
  PageDots dots_;
  float limit_w_{0};
};

}  // namespace esphome::dial_ui
