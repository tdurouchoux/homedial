#pragma once
// ArcGauge ring (design/components/ArcGauge): 270° track, state-coloured indicator, no knob.

#include "widget_common.h"

namespace esphome::dial_ui {

class ArcGauge {
 public:
  // The full 270° ring, or a segment of it: `start` degrees from the ring's start, `sweep` long
  // (the Room page's segmented ring).
  void create(lv_obj_t *parent, int32_t start = 0, int32_t sweep = DIALED_ARC_SWEEP);
  // fraction 0..1; animate eases to the new value in 250 ms (design "Motion").
  void set(float fraction, Status status, bool animate);
  // Empties the ring without animation, so the next animated set() sweeps from 0.
  void reset();

 protected:
  static void anim_cb_(void *arc, int32_t v);
  lv_obj_t *arc_{nullptr};
};

}  // namespace esphome::dial_ui
