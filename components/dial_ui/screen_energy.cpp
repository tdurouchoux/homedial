#include "screen_energy.h"

namespace esphome::dial_ui {

// Tops from design ScreenEnergy.
static constexpr int32_t TITLE_TOP = 24;
static constexpr int32_t TOTAL_TOP = 42;
static constexpr int32_t CHART_TOP = 80;
static constexpr int32_t AXIS_TOP = 158;
static constexpr int32_t AXIS_X = 64;  // axis-end labels, from the centre

void ScreenEnergy::create() {
  this->screen_ = create_screen();
  lv_obj_t *title = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK_MUTED, TITLE_TOP);
  lv_label_set_text(title, "Today");
  this->total_ = create_text(this->screen_, DIALED_FONT_VALUE_M, DIALED_INK, TOTAL_TOP);
  this->chart_.create(this->screen_, CHART_TOP);
  lv_obj_t *left = create_text(this->screen_, DIALED_FONT_CAPTION, DIALED_INK_MUTED, AXIS_TOP, -AXIS_X);
  lv_label_set_text(left, "00h");
  lv_obj_t *right = create_text(this->screen_, DIALED_FONT_CAPTION, DIALED_INK_MUTED, AXIS_TOP, AXIS_X);
  lv_label_set_text(right, "24h");
  EnergyDay none;
  this->update(none, false);
}

void ScreenEnergy::update(const EnergyDay &day, bool online) {
  float values[BarChart::N];
  float peak = 0;
  for (size_t i = 0; i < BarChart::N; i++) {
    values[i] = day.bar(i);
    if (!std::isnan(values[i]) && values[i] > peak)
      peak = values[i];
  }
  this->chart_.set_data(values, day.has_clock() ? day.current_hour() : BarChart::N, nice_axis_max(peak));

  char buf[24];
  char text[32];
  float total = online ? day.today_total() : NAN;
  format_kwh(total, buf, sizeof(buf));
  snprintf(text, sizeof(text), "%s kWh", buf);
  lv_label_set_text(this->total_, text);
}

}  // namespace esphome::dial_ui
