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
  // Arrival nudge: the room name and readings ease in from (dx, dy) px, 30 ms apart
  // (readings in reading order; sideways, the leading side moves first).
  void nudge(int32_t dx, int32_t dy) {
    nudge_in(this->name_, dx, 0);
    for (size_t i = 0; i < this->slot_count_; i++) {
      size_t k = dx < 0 ? this->slot_count_ - 1 - i : i;  // arriving from the left: rightmost last
      nudge_in(this->slots_[i].box, dx, dy, static_cast<uint32_t>(k + (dx != 0 ? 1 : 0)) * 30);
    }
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
  lv_obj_t *name_{nullptr};
  Slot slots_[METRIC_COUNT];
  size_t slot_count_{0};
  PageDots dots_;
};

}  // namespace esphome::dial_ui
