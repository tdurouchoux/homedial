#pragma once
// Sensor page (design/components/ScreenSensor): one metric of one room on the ring gauge.
// Wi-Fi and battery icons are not shown in this iteration (spec.md "Iteration scope").

#include "model_room.h"
#include "widget_arc_gauge.h"
#include "widget_common.h"
#include "widget_location_label.h"
#include "widget_page_dots.h"
#include "widget_status_label.h"

namespace esphome::dial_ui {

class ScreenSensor {
 public:
  void create();
  // Shows reading `index` of `room`; dots count only this room's sensors. The ring sweeps
  // from 0 to the value; `dots_from` (an index, or SIZE_MAX) makes the dots glide from there.
  void bind(const Room &room, size_t room_index, size_t index, bool online, size_t dots_from = SIZE_MAX);
  void update(const Room &room, bool online, bool animate);
  // Arrival from a vertical move: the hero value eases in from `dy` px.
  void nudge_value(int32_t dy) { nudge_in(this->value_, dy); }
  bool is_bound_to(size_t room_index, size_t index) const {
    return this->room_index_ == room_index && this->index_ == index;
  }
  size_t room_index() const { return this->room_index_; }
  size_t index() const { return this->index_; }
  lv_obj_t *screen() const { return this->screen_; }

 protected:
  lv_obj_t *screen_{nullptr};
  lv_obj_t *room_{nullptr};
  lv_obj_t *metric_{nullptr};
  lv_obj_t *value_{nullptr};
  lv_obj_t *unit_{nullptr};
  ArcGauge gauge_;
  StatusLabel status_;
  PageDots dots_;
  size_t room_index_{SIZE_MAX};
  size_t index_{SIZE_MAX};
};

}  // namespace esphome::dial_ui
