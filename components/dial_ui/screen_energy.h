#pragma once
// Energy (design/components/ScreenEnergy, laid out like ScreenHistory): kWh per hour over the last
// 24 h, with scrub. Child of Electricity: no page dots; long press or swipe down returns.

#include "model_energy.h"
#include "widget_bar_chart.h"
#include "widget_common.h"

namespace esphome::dial_ui {

class ScreenEnergy {
 public:
  void create();
  // Live readout (total over the window) and bars; while scrubbing the scrubbed hour stays on
  // screen. Offline: the total reads "--" (the live counter is stale); the bars keep the hours known.
  void update(const EnergyHistory &h, bool online);
  // Scrub readout for bar `i` ("kWh • 21:00", that hour's kWh) with the bar lit; -1 ends it.
  void scrub(const EnergyHistory &h, int i, bool online);
  int scrub_index() const { return this->scrub_; }
  BarChart &chart() { return this->chart_; }
  void nudge(int32_t dy) { nudge_in(this->value_, 0, dy); }
  lv_obj_t *screen() const { return this->screen_; }

 protected:
  void live_readout_(const EnergyHistory &h, bool online);

  lv_obj_t *screen_{nullptr};
  lv_obj_t *label_{nullptr};
  lv_obj_t *value_{nullptr};
  BarChart chart_;
  int scrub_{-1};
};

}  // namespace esphome::dial_ui
