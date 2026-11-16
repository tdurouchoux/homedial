#pragma once
// Energy today (design/components/ScreenEnergy): today's total and 24 hourly bars.
// Child of Electricity: no page dots; long press or swipe down returns.

#include "model_energy.h"
#include "widget_bar_chart.h"
#include "widget_common.h"

namespace esphome::dial_ui {

class ScreenEnergy {
 public:
  void create();
  // Redraws the total and the bars. Offline: the total reads "--" (the live counter is stale);
  // the bars keep the hours already known.
  void update(const EnergyDay &day, bool online);
  void nudge(int32_t dy) { nudge_in(this->total_, 0, dy); }
  lv_obj_t *screen() const { return this->screen_; }

 protected:
  lv_obj_t *screen_{nullptr};
  lv_obj_t *total_{nullptr};
  BarChart chart_;
};

}  // namespace esphome::dial_ui
