#include "screen_sensor.h"

#include <cstring>

namespace esphome::dial_ui {

// Tops from design ScreenSensor.
static constexpr int32_t ROOM_TOP = 44;
static constexpr int32_t METRIC_TOP = 63;
static constexpr int32_t VALUE_TOP = 86;
static constexpr int32_t UNIT_TOP = 142;
static constexpr int32_t STATUS_TOP = 186;
static constexpr size_t VALUE_XL_MAX_CHARS = 5;  // longer readings use value-l (design ArcGauge)

void ScreenSensor::create() {
  this->screen_ = create_screen();
  this->gauge_.create(this->screen_);
  this->room_.create(this->screen_, DIALED_FONT_BODY, ROOM_TOP);
  this->metric_ = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK_MUTED, METRIC_TOP);
  this->value_ = create_text(this->screen_, DIALED_FONT_VALUE_XL, DIALED_INK, VALUE_TOP);
  this->unit_ = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK_MUTED, UNIT_TOP);
  this->status_.create(this->screen_, STATUS_TOP);
  this->dots_.create(this->screen_);
}

void ScreenSensor::bind(const Room &room, size_t room_index, size_t index, bool online, size_t dots_from) {
  this->room_index_ = room_index;
  this->index_ = index;
  const MetricInfo &info = metric_info(room.readings[index].metric);
  this->room_.set(room.name, room.icon);
  lv_label_set_text(this->metric_, info.name);
  lv_label_set_text(this->unit_, info.unit);
  if (dots_from == SIZE_MAX)
    this->dots_.set(room.readings.size(), index);
  else
    this->dots_.animate(room.readings.size(), dots_from, index);
  // Ring sweep on arrival: only the thin ring is redrawn (cheap, unlike a page slide).
  this->gauge_.reset();
  this->update(room, online, true);
}

void ScreenSensor::update(const Room &room, bool online, bool animate) {
  if (this->index_ >= room.readings.size())
    return;
  const Reading &r = room.readings[this->index_];
  char buf[16];
  format_value(buf, sizeof(buf), r.value, r.metric, online);
  Status st = compute_status(display_round(r.value, r.metric), r.thresholds, online);
  lv_obj_set_style_text_font(this->value_,
                             strlen(buf) > VALUE_XL_MAX_CHARS ? DIALED_FONT_VALUE_L : DIALED_FONT_VALUE_XL,
                             LV_PART_MAIN);
  lv_label_set_text(this->value_, buf);
  this->gauge_.set(ring_fraction(r.value, r.thresholds), st, animate);
  this->status_.set(st);
}

}  // namespace esphome::dial_ui
