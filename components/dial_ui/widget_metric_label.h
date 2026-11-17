#pragma once
// MetricLabel (design/components/MetricIcon): a metric's coloured icon in front of its text line,
// centred together as one line. Icon size is chosen per page (Room 12, History 14, Sensor 16 px).

#include "widget_common.h"

extern "C" {
// tools/gen_icons.js -> metric_icons.c (A8 images drawn in the recolour colour).
extern const lv_image_dsc_t dialed_metric_temperature_12, dialed_metric_humidity_12, dialed_metric_air_quality_12,
    dialed_metric_temperature_14, dialed_metric_humidity_14, dialed_metric_air_quality_14,
    dialed_metric_temperature_16, dialed_metric_humidity_16, dialed_metric_air_quality_16;
}

namespace esphome::dial_ui {

enum class MetricIconSize : uint8_t { ROOM = 0, HISTORY = 1, SENSOR = 2 };  // 12, 14, 16 px

inline const lv_image_dsc_t *metric_icon_src(Metric m, MetricIconSize size) {
  static const lv_image_dsc_t *const ICONS[3][METRIC_COUNT] = {
      {&dialed_metric_temperature_12, &dialed_metric_humidity_12, &dialed_metric_air_quality_12},
      {&dialed_metric_temperature_14, &dialed_metric_humidity_14, &dialed_metric_air_quality_14},
      {&dialed_metric_temperature_16, &dialed_metric_humidity_16, &dialed_metric_air_quality_16},
  };
  return ICONS[static_cast<size_t>(size)][static_cast<size_t>(m)];
}

inline lv_color_t metric_color(Metric m) {
  switch (m) {
    case Metric::TEMPERATURE:
      return DIALED_METRIC_TEMP;
    case Metric::HUMIDITY:
      return DIALED_METRIC_HUMIDITY;
    default:
      return DIALED_METRIC_AIR;
  }
}

class MetricLabel {
 public:
  // `top` is the line's top edge, centred on `parent` (plus `x_offset`).
  void create(lv_obj_t *parent, const lv_font_t *font, MetricIconSize size, int32_t gap, int32_t top) {
    this->size_ = size;
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, gap, LV_PART_MAIN);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, top);
    this->icon_ = lv_image_create(row);
    lv_obj_remove_flag(this->icon_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_image_recolor_opa(this->icon_, LV_OPA_COVER, LV_PART_MAIN);
    this->text_ = lv_label_create(row);
    lv_obj_set_style_text_font(this->text_, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->text_, DIALED_INK_MUTED, LV_PART_MAIN);
    this->row_ = row;
  }

  // Sets the icon (and its colour) for `m`; the text is set with set_text() / text().
  void set_metric(Metric m) {
    lv_image_set_src(this->icon_, metric_icon_src(m, this->size_));
    lv_obj_set_style_image_recolor(this->icon_, metric_color(m), LV_PART_MAIN);
  }
  lv_obj_t *text() const { return this->text_; }
  lv_obj_t *obj() const { return this->row_; }

 protected:
  lv_obj_t *row_{nullptr};
  lv_obj_t *icon_{nullptr};
  lv_obj_t *text_{nullptr};
  MetricIconSize size_{MetricIconSize::ROOM};
};

}  // namespace esphome::dial_ui
