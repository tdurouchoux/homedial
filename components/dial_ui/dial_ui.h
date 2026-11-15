#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

namespace esphome::dial_ui {

class DialUi : public Component, public Parented<lvgl::LvglComponent> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  // After LvglComponent (setup_priority::PROCESSOR), so LVGL and the display are ready.
  float get_setup_priority() const override { return setup_priority::PROCESSOR - 10.0f; }

 protected:
  lv_obj_t *screen_{nullptr};
};

}  // namespace esphome::dial_ui
