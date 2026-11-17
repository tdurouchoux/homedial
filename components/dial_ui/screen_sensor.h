#pragma once
// Sensor page (design/components/ScreenSensor): one metric of one room on the ring gauge, with
// a 24 h trend line under the value when the metric has history.
// Wi-Fi and battery icons are not shown in this iteration (spec.md "Iteration scope").

#include "model_room.h"
#include "widget_arc_gauge.h"
#include "widget_common.h"
#include "widget_location_label.h"
#include "widget_metric_label.h"
#include "widget_page_dots.h"
#include "widget_sparkline.h"
#include "widget_status_label.h"

namespace esphome::dial_ui {

class ScreenSensor {
 public:
  void create();
  // Shows reading `index` of `room`; dots count only this room's sensors. The ring sweeps
  // from 0 to the value; `dots_from` (an index, or SIZE_MAX) makes the dots glide from there.
  void bind(const Room &room, size_t room_index, size_t index, bool online, size_t dots_from = SIZE_MAX);
  void update(const Room &room, bool online, bool animate);
  // Redraws the trend line from the reading's history (after the minute sample).
  void update_trend(const Room &room) {
    if (this->index_ < room.readings.size())
      this->trend_.set(room.readings[this->index_]);
  }
  // Arrival nudge: vertical moves nudge the value; sideways moves (metric to metric) nudge
  // the metric line, value and trend line (the room line stays: same room).
  void nudge(int32_t dx, int32_t dy) {
    nudge_in(this->metric_.obj(), dx, 0);
    nudge_in(this->value_, dx, dy);
    nudge_in(this->trend_.obj(), dx, 0, dx != 0 ? 30 : 0);
  }
  bool is_bound_to(size_t room_index, size_t index) const {
    return this->room_index_ == room_index && this->index_ == index;
  }
  size_t room_index() const { return this->room_index_; }
  size_t index() const { return this->index_; }
  lv_obj_t *screen() const { return this->screen_; }
  lv_obj_t *value_obj() const { return this->value_; }  // tap target: opens History

 protected:
  lv_obj_t *screen_{nullptr};
  LocationLabel room_;
  MetricLabel metric_;
  lv_obj_t *value_{nullptr};
  ArcGauge gauge_;
  Sparkline trend_;
  StatusLabel status_;
  PageDots dots_;
  size_t room_index_{SIZE_MAX};
  size_t index_{SIZE_MAX};
};

}  // namespace esphome::dial_ui
