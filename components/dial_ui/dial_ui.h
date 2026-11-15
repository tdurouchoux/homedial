#pragma once
// dial_ui: the Dialed UI on ESPHome's LVGL instance. Data contract: contract.md.

#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "esphome/components/sensor/sensor.h"

#include "model_room.h"
#include "screen_room.h"
#include "screen_sensor.h"

namespace esphome::dial_ui {

class DialUi : public Component, public Parented<lvgl::LvglComponent> {
 public:
  // ---- Codegen (packages/ui.yaml -> __init__.py) ----
  void add_room(const char *id, const char *name);
  void add_reading(Metric metric, sensor::Sensor *sensor, Thresholds thresholds);  // to the last room

  // ---- Escape hatch for YAML lambdas: id(ui).set_reading("living", "temperature", x) ----
  void set_reading(const char *room_id, const char *metric, float value);

  void setup() override;
  void loop() override;
  void dump_config() override;
  // After LvglComponent (setup_priority::PROCESSOR), so LVGL and the display are ready.
  float get_setup_priority() const override { return setup_priority::PROCESSOR - 10.0f; }

 protected:
  enum class Level : uint8_t { ROOM, SENSOR };

  void set_value_(size_t room, size_t index, float value);
  bool online_() const;
  void refresh_();
  void show_room_(size_t room);
  void show_sensor_(size_t room, size_t index);
  void load_(lv_obj_t *screen);
  void attach_events_(lv_obj_t *screen);
  static void event_cb_(lv_event_t *e);
  void on_event_(lv_event_t *e);

  std::vector<Room> rooms_;
  std::vector<ScreenRoom> room_screens_;
  ScreenSensor sensor_screen_;

  Level level_{Level::ROOM};
  size_t room_{0};
  bool online_last_{false};
  uint32_t last_refresh_ms_{0};
};

}  // namespace esphome::dial_ui
