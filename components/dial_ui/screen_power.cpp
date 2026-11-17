#include "screen_power.h"

#include <cstring>

namespace esphome::dial_ui {

// Tops from design ScreenPower.
static constexpr int32_t TITLE_TOP = 54;
static constexpr int32_t VALUE_TOP = 84;
static constexpr int32_t UNIT_TOP = 140;
static constexpr int32_t LIMIT_TOP = 158;
static constexpr int32_t STATUS_TOP = 186;

void ScreenPower::create(float limit_w, bool tappable) {
  this->limit_w_ = limit_w;
  this->screen_ = create_screen();
  this->gauge_.create(this->screen_);
  MetricLabel title;  // the widgets live on the screen; the helper only builds them
  title.create(this->screen_, DIALED_FONT_LABEL, MetricIconSize::SENSOR, 6, TITLE_TOP);
  title.set_power();
  lv_label_set_text(title.text(), "Power now");
  this->value_ = create_text(this->screen_, DIALED_FONT_VALUE_XL, DIALED_INK, VALUE_TOP);
  if (tappable)
    make_tappable(this->value_);  // a tap opens Energy (spec.md)
  this->unit_ = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK_MUTED, UNIT_TOP);
  lv_obj_t *limit = create_text(this->screen_, DIALED_FONT_CAPTION, DIALED_INK_MUTED, LIMIT_TOP);
  lv_label_set_text_fmt(limit, "limit %d W", static_cast<int>(limit_w + 0.5f));
  this->status_.create(this->screen_, STATUS_TOP);
  this->dots_.create(this->screen_);
  this->update(NAN, false, false);
}

void ScreenPower::update(float watts, bool online, bool animate) {
  char buf[16];
  const char *unit;
  format_power(watts, online, buf, sizeof(buf), &unit);
  lv_label_set_text(this->value_, buf);
  lv_label_set_text(this->unit_, unit);
  Status st = power_status(watts, this->limit_w_, online);
  this->gauge_.set(power_fraction(watts, this->limit_w_), st, animate);
  this->status_.set(st);
}

}  // namespace esphome::dial_ui
