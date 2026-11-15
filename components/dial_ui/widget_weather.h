#pragma once
// Weather widgets: WeatherIcon (design/components/WeatherIcon) and the next-hour rain arc
// (spec.md "Weather": the ring is a 0 -> 60 min timeline, rainy stretches as `accent` segments).

#include "model_weather.h"
#include "widget_common.h"

extern "C" {
// tools/gen_weather_icons.js -> weather_icons.c (A8 images drawn in the recolour colour).
extern const lv_image_dsc_t dialed_weather_clear_48, dialed_weather_part_cloudy_48, dialed_weather_cloudy_48,
    dialed_weather_rain_48, dialed_weather_snow_48, dialed_weather_storm_48;
extern const lv_image_dsc_t dialed_weather_clear_32, dialed_weather_part_cloudy_32, dialed_weather_cloudy_32,
    dialed_weather_rain_32, dialed_weather_snow_32, dialed_weather_storm_32;
}

namespace esphome::dial_ui {

inline const lv_image_dsc_t *weather_icon_src(WeatherIcon icon, bool large) {
  static const lv_image_dsc_t *const L[] = {&dialed_weather_clear_48, &dialed_weather_part_cloudy_48,
                                            &dialed_weather_cloudy_48, &dialed_weather_rain_48,
                                            &dialed_weather_snow_48,  &dialed_weather_storm_48};
  static const lv_image_dsc_t *const M[] = {&dialed_weather_clear_32, &dialed_weather_part_cloudy_32,
                                            &dialed_weather_cloudy_32, &dialed_weather_rain_32,
                                            &dialed_weather_snow_32,  &dialed_weather_storm_32};
  return (large ? L : M)[static_cast<size_t>(icon)];
}

// An lv_image showing an A8 icon in `ink` (the image recolour is the mask colour).
inline lv_obj_t *create_weather_icon(lv_obj_t *parent) {
  lv_obj_t *img = lv_image_create(parent);
  lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_image_recolor(img, DIALED_INK, LV_PART_MAIN);
  lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, LV_PART_MAIN);
  return img;
}

class RainArc {
 public:
  static constexpr size_t MAX_RUNS = 6;
  void create(lv_obj_t *parent);
  // `available`: the next-hour rain forecast is known. Then the track is shown even when the
  // hour is dry (user, 2026-11-15; the design hid it); with no rain data there is no ring.
  void set(const RainRun *runs, size_t n, bool available);

 protected:
  lv_obj_t *track_{nullptr};
  lv_obj_t *runs_[MAX_RUNS]{};
};

}  // namespace esphome::dial_ui
