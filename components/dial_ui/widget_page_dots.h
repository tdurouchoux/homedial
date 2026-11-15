#pragma once
// PageDots (design/components/PageDots): 8 px dots in `line`, the active one an 18 x 8 bar in `ink`.

#include "widget_common.h"

namespace esphome::dial_ui {

class PageDots {
 public:
  void create(lv_obj_t *parent);
  // Hidden when count < 2. Up to 5: 8 px gaps at top 222; 6-7: 6 px gaps at top 218.
  void set(size_t count, size_t active);

 protected:
  static constexpr size_t MAX_DOTS = 7;
  lv_obj_t *row_{nullptr};
  lv_obj_t *dots_[MAX_DOTS]{};
};

}  // namespace esphome::dial_ui
