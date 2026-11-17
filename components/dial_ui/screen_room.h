#pragma once
// Room summary page (design/components/ScreenRoom): room name, 1-3 readings, page dots.

#include "model_room.h"
#include "widget_common.h"
#include "widget_location_label.h"
#include "widget_metric_label.h"
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
    nudge_in(this->name_.obj(), dx, 0);
    for (size_t i = 0; i < this->slot_count_; i++) {
      // Sideways, the stagger follows horizontal position (leading side first), so the centred top
      // reading of a 3-reading room moves between the left and right ones, not before or after both.
      size_t k = i;
      if (dx != 0) {
        size_t rank = 0;  // readings to the left of this one
        for (size_t j = 0; j < this->slot_count_; j++)
          if (this->slot_x_[j] < this->slot_x_[i])
            rank++;
        k = (dx < 0 ? this->slot_count_ - 1 - rank : rank) + 1;
      }
      nudge_in(this->slots_[i].box, dx, dy, static_cast<uint32_t>(k) * 30);
    }
  }
  lv_obj_t *screen() const { return this->screen_; }
  // Reading slot that was tapped, or -1.
  int slot_index(lv_obj_t *obj) const;

 protected:
  struct Slot {
    lv_obj_t *box{nullptr};    // clickable target (>= 82 x 58)
    MetricLabel label;         // coloured dot + metric + unit
    lv_obj_t *value{nullptr};
  };
  lv_obj_t *screen_{nullptr};
  LocationLabel name_;
  Slot slots_[METRIC_COUNT];
  int32_t slot_x_[METRIC_COUNT]{};  // reading centres (x), for the sideways stagger
  size_t slot_count_{0};
  PageDots dots_;
};

}  // namespace esphome::dial_ui
