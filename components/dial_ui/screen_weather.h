#pragma once
// Weather now (design/components/ScreenWeather; spec.md "Weather"): icon, temperature,
// condition word, next-hour rain arc and rain line, page dots.

#include "model_weather.h"
#include "widget_common.h"
#include "widget_page_dots.h"
#include "widget_arc_scale.h"
#include "widget_status_label.h"
#include "widget_weather.h"

namespace esphome::dial_ui {

class ScreenWeather {
 public:
  void create();
  // temp: NaN when missing; condition: HA string; runs: next-hour rain (n = 0: dry);
  // rain_known: the rain forecast is available (shows the empty track when dry).
  void update(float temp, const std::string &condition, const RainRun *runs, size_t n, bool rain_known,
              bool online);
  void set_dots(size_t count, size_t active) { this->dots_.set(count, active); }
  void animate_dots(size_t count, size_t from, size_t active) { this->dots_.animate(count, from, active); }
  void nudge(int32_t dx, int32_t dy) {
    nudge_in(this->icon_, dx, dy);
    nudge_in(this->temp_, dx, dy, 30);
    nudge_in(this->word_, dx, 0, 60);
  }
  lv_obj_t *screen() const { return this->screen_; }

 protected:
  lv_obj_t *screen_{nullptr};
  lv_obj_t *icon_{nullptr};
  lv_obj_t *temp_{nullptr};
  lv_obj_t *word_{nullptr};
  lv_obj_t *rain_{nullptr};
  RainArc arc_;
  ArcScale scale_;  // 15 / 30 / 45 min, only with the ring
  StatusLabel status_;  // OFFLINE in place of the condition word when data is missing
  PageDots dots_;
};

}  // namespace esphome::dial_ui
