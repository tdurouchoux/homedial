#pragma once
// dial_ui: the Dialed UI on ESPHome's LVGL instance. Data contract: contract.md.

#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"

#include "model_nav.h"
#include "model_weather.h"
#include "model_room.h"
#include "screen_forecast.h"
#include "screen_history.h"
#include "screen_room.h"
#include "screen_sensor.h"
#include "screen_settings.h"
#include "screen_weather.h"

namespace esphome::dial_ui {

class DialUi : public Component, public Parented<lvgl::LvglComponent> {
 public:
  // ---- Codegen (packages/ui.yaml -> __init__.py) ----
  void add_room(const char *id, const char *name);
  void add_reading(Metric metric, sensor::Sensor *sensor, Thresholds thresholds, bool history);  // to the last room
  void set_time(time::RealTimeClock *time) { this->time_ = time; }
  // Weather now (spec.md "Weather"): temperature attribute, condition state, and the
  // Météo-France Next rain attributes `1_hour_forecast` / `forecast_time_ref` (both optional).
  void set_weather(sensor::Sensor *temperature, text_sensor::TextSensor *condition);
  void set_rain(text_sensor::TextSensor *forecast, text_sensor::TextSensor *forecast_time);
  // YAML `on_forecast_request:` runs weather.get_forecasts and hands the compacted rows back:
  //   on_success: - lambda: id(ui).load_forecast(response["response"]);
  Trigger<> *get_forecast_request_trigger() { return &this->forecast_request_trigger_; }
  void load_forecast(JsonVariantConst rows);
  void forecast_request_failed(const std::string &error);

  // ---- Escape hatch for YAML lambdas: id(ui).set_reading("living", "temperature", x) ----
  void set_reading(const char *room_id, const char *metric, float value);

  void setup() override;
  void loop() override;
  void dump_config() override;
  // After LvglComponent (setup_priority::PROCESSOR), so LVGL and the display are ready.
  float get_setup_priority() const override { return setup_priority::PROCESSOR - 10.0f; }

 protected:
  void set_value_(size_t room, size_t index, float value);
  bool online_() const;
  void refresh_();
  void sample_history_();
  int64_t now_epoch_() const;
  void refresh_weather_();
  void maybe_request_forecast_(bool force);

  // History page: scrub with the encoder (press, turn) or touch (hold 250 ms on the chart, drag).
  bool history_event_(lv_event_t *e, lv_event_code_t code, bool from_encoder);
  static void chart_event_cb_(lv_event_t *e);
  void on_chart_event_(lv_event_t *e);
  static void scrub_hold_cb_(lv_timer_t *t);
  static void scrub_revert_cb_(lv_timer_t *t);
  void scrub_to_(int index);
  void end_scrub_();

  // Navigation: every input becomes a NavAction (model_nav.h).
  void nav_(NavAction action);
  void open_metric_(size_t room, size_t index);  // tap on a Room reading
  NavShape shape_() const;
  void show_(const Page &page, Slide slide);
  void load_(lv_obj_t *screen, Slide slide);
  ScreenSensor *visible_sensor_() { return &this->sensor_screens_[this->sensor_active_]; }

  void attach_events_(lv_obj_t *screen);
  static void event_cb_(lv_event_t *e);
  void on_event_(lv_event_t *e);
  static void hold_timer_cb_(lv_timer_t *t);
  void cancel_hold_();

  std::vector<Room> rooms_;
  std::vector<size_t> readings_per_room_;
  std::vector<ScreenRoom> room_screens_;
  // Two Sensor screens, so a slide between two metrics has a screen to leave and one to enter.
  ScreenSensor sensor_screens_[2];
  size_t sensor_active_{0};
  ScreenSettings settings_screen_;
  ScreenHistory history_screen_;
  ScreenWeather weather_screen_;
  ScreenForecast forecast_screen_;

  // Weather state (written by callbacks, drawn by loop()).
  bool has_weather_{false};
  float outdoor_temp_{NAN};
  std::string condition_;
  RainForecast rain_;
  std::string rain_attr_;
  bool weather_dirty_{true};
  uint32_t last_rain_minute_{0};
  std::vector<ForecastHour> forecast_;
  bool forecast_dirty_{false};
  uint32_t forecast_loaded_ms_{0};
  bool forecast_loaded_{false};
  bool forecast_pending_{false};
  uint32_t forecast_requested_ms_{0};
  Trigger<> forecast_request_trigger_;
  std::vector<uint8_t> history_mask_;  // per room, bit i = reading i has history
  time::RealTimeClock *time_{nullptr};
  uint32_t last_sample_ms_{0};
  bool sampled_once_{false};

  Page page_{};
  Page before_settings_{};
  bool online_last_{false};
  uint32_t last_refresh_ms_{0};

  // Touch hold 600 ms at the top level opens Settings (spec.md).
  lv_timer_t *hold_timer_{nullptr};
  lv_indev_t *hold_indev_{nullptr};
  lv_point_t hold_point_{};

  bool scrub_encoder_{false};  // encoder scrub in progress
  bool scrub_touch_{false};    // touch scrub in progress (finger down)
  lv_timer_t *scrub_hold_timer_{nullptr};
  lv_timer_t *scrub_revert_timer_{nullptr};
  lv_indev_t *scrub_indev_{nullptr};
  lv_point_t scrub_point_{};
};

}  // namespace esphome::dial_ui
