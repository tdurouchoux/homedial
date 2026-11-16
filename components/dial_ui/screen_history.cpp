#include "screen_history.h"

#include "esphome/core/time.h"

namespace esphome::dial_ui {

// Tops from design ScreenHistory.
static constexpr int32_t ROOM_TOP = 28;
static constexpr int32_t METRIC_TOP = 46;
static constexpr int32_t VALUE_TOP = 62;
static constexpr int32_t CHART_TOP = 98;
static constexpr int32_t AXIS_TOP = 174;
static constexpr int32_t STATUS_TOP = 190;
static constexpr int32_t AXIS_X = 64;  // axis-end labels, from the centre

void ScreenHistory::create() {
  this->screen_ = create_screen();
  this->room_.create(this->screen_, DIALED_FONT_BODY, ROOM_TOP);
  this->metric_ = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK_MUTED, METRIC_TOP);
  this->value_ = create_text(this->screen_, DIALED_FONT_VALUE_M, DIALED_INK, VALUE_TOP);
  this->chart_.create(this->screen_, CHART_TOP);
  lv_obj_t *left = create_text(this->screen_, DIALED_FONT_CAPTION, DIALED_INK_MUTED, AXIS_TOP, -AXIS_X);
  lv_label_set_text(left, "-24h");
  lv_obj_t *right = create_text(this->screen_, DIALED_FONT_CAPTION, DIALED_INK_MUTED, AXIS_TOP, AXIS_X);
  lv_label_set_text(right, "now");
  this->status_.create(this->screen_, STATUS_TOP);
}

void ScreenHistory::bind(const Room &room, size_t room_index, size_t index, bool online) {
  this->room_index_ = room_index;
  this->index_ = index;
  this->scrub_ = -1;
  this->chart_.show_cursor(-1);
  this->room_.set(room.name, room.icon);
  this->update(room, online);
}

void ScreenHistory::live_readout_(const Room &room, bool online) {
  const Reading &r = room.readings[this->index_];
  lv_label_set_text_fmt(this->metric_, "%s " LV_SYMBOL_BULLET " 24 h", metric_info(r.metric).short_label);
  char buf[16];
  format_value(buf, sizeof(buf), r.value, r.metric, online);
  lv_label_set_text(this->value_, buf);
  this->status_.set(compute_status(display_round(r.value, r.metric), r.thresholds, online));
}

void ScreenHistory::update(const Room &room, bool online) {
  if (this->index_ >= room.readings.size())
    return;
  const Reading &r = room.readings[this->index_];
  this->chart_.set_data(r.history, r);
  if (this->scrub_ >= 0) {
    this->chart_.show_cursor(this->scrub_);  // keep cursor and dot on the (redrawn) point
    return;
  }
  this->live_readout_(room, online);
}

void ScreenHistory::scrub(const Room &room, int i, bool online) {
  if (this->index_ >= room.readings.size())
    return;
  if (i < 0 || i >= static_cast<int>(HourlyHistory::POINTS)) {
    this->scrub_ = -1;
    this->chart_.show_cursor(-1);
    this->live_readout_(room, online);
    return;
  }
  this->scrub_ = i;
  this->chart_.show_cursor(i);
  const Reading &r = room.readings[this->index_];
  float v = r.history.point(static_cast<size_t>(i));
  // Time of the sample: the hour it averages, in local time ("now" for the current hour).
  char when[8] = "now";
  if (i < static_cast<int>(HourlyHistory::POINTS) - 1) {
    ESPTime lt = ESPTime::from_epoch_local(static_cast<time_t>(r.history.hour_start(static_cast<size_t>(i))));
    snprintf(when, sizeof(when), "%02d:00", lt.hour);
  }
  lv_label_set_text_fmt(this->metric_, "%s " LV_SYMBOL_BULLET " %s", metric_info(r.metric).short_label, when);
  char buf[16];
  format_value(buf, sizeof(buf), v, r.metric, true);
  lv_label_set_text(this->value_, buf);
  // A past sample's own state; a missing hour reads OFFLINE.
  this->status_.set(compute_status(display_round(v, r.metric), r.thresholds, true));
}

}  // namespace esphome::dial_ui
