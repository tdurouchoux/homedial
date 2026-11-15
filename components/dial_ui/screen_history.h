#pragma once
// History page (design/components/ScreenHistory): one metric of one room over 24 h, with scrub.

#include "model_history.h"
#include "model_room.h"
#include "widget_common.h"
#include "widget_location_label.h"
#include "widget_status_label.h"
#include "widget_trend_chart.h"

namespace esphome::dial_ui {

class ScreenHistory {
 public:
  void create();
  void bind(const Room &room, size_t room_index, size_t index, bool online);
  // Live readout and chart (not while scrubbing: the scrubbed sample stays on screen).
  void update(const Room &room, bool online);
  // Scrub readout for point `i` ("Temp °C • 21:00", that hour's mean and state); -1 ends it.
  void scrub(const Room &room, int i, bool online);
  bool is_bound_to(size_t room_index, size_t index) const {
    return this->room_index_ == room_index && this->index_ == index;
  }
  size_t index() const { return this->index_; }
  int scrub_index() const { return this->scrub_; }
  TrendChart &chart() { return this->chart_; }
  lv_obj_t *screen() const { return this->screen_; }
  void nudge(int32_t dy) { nudge_in(this->value_, 0, dy); }

 protected:
  void live_readout_(const Room &room, bool online);

  lv_obj_t *screen_{nullptr};
  lv_obj_t *room_{nullptr};
  lv_obj_t *metric_{nullptr};
  lv_obj_t *value_{nullptr};
  TrendChart chart_;
  StatusLabel status_;
  size_t room_index_{SIZE_MAX};
  size_t index_{SIZE_MAX};
  int scrub_{-1};
};

}  // namespace esphome::dial_ui
