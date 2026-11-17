#include "screen_energy.h"

#include "esphome/core/time.h"

namespace esphome::dial_ui {

// Same tops as ScreenHistory, so the two 24 h views read alike.
static constexpr int32_t TITLE_TOP = 28;
static constexpr int32_t LABEL_TOP = 46;
static constexpr int32_t VALUE_TOP = 62;
static constexpr int32_t CHART_TOP = 98;
static constexpr int32_t AXIS_TOP = 174;
static constexpr int32_t AXIS_X = 64;  // axis-end labels, from the centre

void ScreenEnergy::create() {
  this->screen_ = create_screen();
  lv_obj_t *title = create_text(this->screen_, DIALED_FONT_BODY, DIALED_INK, TITLE_TOP);
  lv_label_set_text(title, "Energy");
  this->label_ = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK_MUTED, LABEL_TOP);
  this->value_ = create_text(this->screen_, DIALED_FONT_VALUE_M, DIALED_INK, VALUE_TOP);
  this->chart_.create(this->screen_, CHART_TOP);
  lv_obj_t *left = create_text(this->screen_, DIALED_FONT_CAPTION, DIALED_INK_MUTED, AXIS_TOP, -AXIS_X);
  lv_label_set_text(left, "-24h");
  lv_obj_t *right = create_text(this->screen_, DIALED_FONT_CAPTION, DIALED_INK_MUTED, AXIS_TOP, AXIS_X);
  lv_label_set_text(right, "now");
  EnergyHistory none;
  this->update(none, false);
}

void ScreenEnergy::live_readout_(const EnergyHistory &h, bool online) {
  lv_label_set_text(this->label_, "kWh " LV_SYMBOL_BULLET " 24 h");
  char buf[16];
  format_kwh(online ? h.total() : NAN, buf, sizeof(buf));
  lv_label_set_text(this->value_, buf);
  this->chart_.highlight(static_cast<int>(EnergyHistory::POINTS) - 1);  // the current hour
}

void ScreenEnergy::update(const EnergyHistory &h, bool online) {
  float values[BarChart::N];
  float peak = 0;
  for (size_t i = 0; i < BarChart::N; i++) {
    values[i] = h.point(i);
    if (!std::isnan(values[i]) && values[i] > peak)
      peak = values[i];
  }
  this->chart_.set_data(values, nice_axis_max(peak));
  if (this->scrub_ >= 0) {
    this->scrub(h, this->scrub_, online);  // the readout follows the (possibly backfilled) hour
    return;
  }
  this->live_readout_(h, online);
}

void ScreenEnergy::scrub(const EnergyHistory &h, int i, bool online) {
  if (i < 0 || i >= static_cast<int>(EnergyHistory::POINTS)) {
    this->scrub_ = -1;
    this->live_readout_(h, online);
    return;
  }
  this->scrub_ = i;
  this->chart_.highlight(i);
  // Time of the bar: the hour it covers, in local time ("now" for the current hour).
  char when[8] = "now";
  if (i < static_cast<int>(EnergyHistory::POINTS) - 1 && h.has_clock()) {
    ESPTime lt = ESPTime::from_epoch_local(static_cast<time_t>(h.hour_start(static_cast<size_t>(i))));
    snprintf(when, sizeof(when), "%02d:00", lt.hour);
  }
  lv_label_set_text_fmt(this->label_, "kWh " LV_SYMBOL_BULLET " %s", when);
  char buf[16];
  format_kwh(h.point(static_cast<size_t>(i)), buf, sizeof(buf));  // a missing hour reads "--"
  lv_label_set_text(this->value_, buf);
}

}  // namespace esphome::dial_ui
