#pragma once
// Room summary page (design/components/ScreenRoom): room name, 1-3 readings, page dots.

#include "model_room.h"
#include "widget_common.h"
#include "widget_location_label.h"
#include "widget_page_dots.h"

namespace esphome::dial_ui {

class ScreenRoom {
 public:
  void create(const Room &room);
  void update_reading(const Room &room, size_t index, bool online);
  void set_dots(size_t count, size_t active) { this->dots_.set(count, active); }
  void animate_dots(size_t count, size_t from, size_t active) { this->dots_.animate(count, from, active); }
  // Arrival from a vertical move: the readings ease in from `dy` px, 30 ms apart.
  void nudge_slots(int32_t dy) {
    for (size_t i = 0; i < this->slot_count_; i++)
      nudge_in(this->slots_[i].box, dy, static_cast<uint32_t>(i) * 30);
  }
  lv_obj_t *screen() const { return this->screen_; }
  // Reading slot that was tapped, or -1.
  int slot_index(lv_obj_t *obj) const;

 protected:
  struct Slot {
    lv_obj_t *box{nullptr};    // clickable target (>= 82 x 58)
    lv_obj_t *label{nullptr};  // metric + unit
    lv_obj_t *value{nullptr};
  };
  lv_obj_t *screen_{nullptr};
  Slot slots_[METRIC_COUNT];
  size_t slot_count_{0};
  PageDots dots_;
};

}  // namespace esphome::dial_ui
