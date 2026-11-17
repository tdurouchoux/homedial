#include "screen_weather.h"

#include <cmath>

namespace esphome::dial_ui {

// Tops from design ScreenWeather.
static constexpr int32_t ICON_TOP = 41;  // design 34, moved down to clear the scale label
static constexpr int32_t TEMP_TOP = 90;
static constexpr int32_t WORD_TOP = 148;
static constexpr int32_t RAIN_TOP = 177;

void ScreenWeather::create() {
  this->screen_ = create_screen();
  this->arc_.create(this->screen_);
  this->scale_.create(this->screen_, 5);  // every 15 min of the next hour
  const char *const marks[] = {"15", "30", "45"};
  const float fracs[] = {0.25f, 0.5f, 0.75f};
  for (size_t i = 0; i < 3; i++) {
    this->scale_.add_label(fracs[i], SCALE_LABEL_MID_R);
    this->scale_.set_label(i, marks[i]);
  }
  this->scale_.set_visible(false);
  this->icon_ = create_weather_icon(this->screen_);
  lv_obj_align(this->icon_, LV_ALIGN_TOP_MID, 0, ICON_TOP);
  this->temp_ = create_text(this->screen_, DIALED_FONT_VALUE_XL, DIALED_INK, TEMP_TOP);
  this->word_ = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK_MUTED, WORD_TOP);
  this->status_.create(this->screen_, WORD_TOP);
  this->rain_ = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK, RAIN_TOP);
  this->dots_.create(this->screen_);
  this->update(NAN, "", nullptr, 0, false, false);
}

void ScreenWeather::update(float temp, const std::string &condition, const RainRun *runs, size_t n, bool rain_known,
                           bool online) {
  ConditionInfo ci;
  bool known = online && condition_info(condition.c_str(), &ci);
  bool has_temp = online && !std::isnan(temp);

  if (known) {
    lv_image_set_src(this->icon_, weather_icon_src(ci.icon, true));
    lv_obj_remove_flag(this->icon_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(this->word_, ci.word);
  } else {
    lv_obj_add_flag(this->icon_, LV_OBJ_FLAG_HIDDEN);
  }
  if (has_temp) {
    // Whole degrees with "°" and no unit (the unit is HA's setting; design ScreenWeather).
    lv_label_set_text_fmt(this->temp_, "%d\xC2\xB0", static_cast<int>(std::lround(temp)));
  } else {
    lv_label_set_text(this->temp_, "--");
  }
  // Missing data: "--" with the OFFLINE status in place of the condition word (design rule).
  bool complete = known && has_temp;
  if (complete)
    lv_obj_remove_flag(this->word_, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(this->word_, LV_OBJ_FLAG_HIDDEN);
  this->status_.set(Status::OFFLINE);
  this->status_.set_visible(!complete);

  size_t shown = online ? n : 0;
  this->arc_.set(runs, shown, online && rain_known);
  this->scale_.set_visible(online && rain_known);  // the scale goes with the ring
  char line[24];
  rain_line(runs, shown, line, sizeof line);
  lv_label_set_text(this->rain_, line);  // empty when dry: no "No rain" (design)
}

}  // namespace esphome::dial_ui
