#pragma once
// Hourly forecast (design/components/ScreenForecast): a strip of 24 hours, the centred column
// is the focus. Turn moves the focus one hour; drag scrolls and snaps; tap centres a column.

#include <vector>

#include "model_weather.h"
#include "widget_common.h"
#include "widget_status_label.h"
#include "widget_weather.h"

namespace esphome::dial_ui {

class ScreenForecast {
 public:
  static constexpr size_t MAX_HOURS = 24;

  void create();
  // rows from forecast_first(); `now` decides "Now" and Today / Tomorrow. Keeps the focus.
  void set_data(const ForecastHour *rows, size_t n, int64_t now);
  // Focus hour `i` and centre its column; `animate` scrolls the strip there (encoder, tap)
  // so the movement reads like a swipe, otherwise it jumps (opening the page).
  void focus(size_t i, bool animate = false);
  void step(int delta);
  size_t focused() const { return this->focus_; }
  lv_obj_t *screen() const { return this->screen_; }
  void nudge(int32_t dy) { nudge_in(this->strip_, 0, dy); }

 protected:
  struct Column {
    lv_obj_t *box{nullptr};
    lv_obj_t *hour{nullptr};
    lv_obj_t *icon{nullptr};
    lv_obj_t *temp{nullptr};
    lv_obj_t *bar{nullptr};
  };
  static void strip_event_cb_(lv_event_t *e);
  static void column_event_cb_(lv_event_t *e);
  void apply_focus_(size_t i);

  lv_obj_t *screen_{nullptr};
  lv_obj_t *day_{nullptr};
  lv_obj_t *word_{nullptr};
  lv_obj_t *strip_{nullptr};
  Column cols_[MAX_HOURS];
  StatusLabel status_;
  ForecastHour rows_[MAX_HOURS]{};
  size_t count_{0};
  size_t focus_{0};
  int64_t now_{0};
};

}  // namespace esphome::dial_ui
