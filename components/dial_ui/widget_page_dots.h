#pragma once
// PageDots (design/components/PageDots): 8 px dots in `line`, the active one an 18 x 8 bar in `ink`.
// Page changes are instant (spec.md), so the dots carry the motion instead: the bar glides from
// the previous page to the new one in 150 ms. Only the ~100 x 8 px row is redrawn.

#include "widget_common.h"

namespace esphome::dial_ui {

class PageDots {
 public:
  void create(lv_obj_t *parent);
  // Hidden when count < 2. Up to 5: 8 px gaps at top 222; 6-7: 6 px gaps at top 218.
  void set(size_t count, size_t active);
  // Same as set(), but the bar glides from `from` to `active`.
  void animate(size_t count, size_t from, size_t active);

 protected:
  static constexpr size_t MAX_DOTS = 7;
  static void anim_cb_(void *self, int32_t v);
  void place_(size_t count);
  void layout_(int32_t pos_x256);  // bar position in 1/256 of a dot index

  lv_obj_t *row_{nullptr};
  lv_obj_t *dots_[MAX_DOTS]{};
  size_t count_{0};
  int32_t gap_{8};
};

}  // namespace esphome::dial_ui
