#pragma once
// ArcScale (design/components/ArcGauge "Scale"): a minimal scale just inside the 270 degree ring:
// short ticks (min, middle, max; or every quarter) in ink-muted at low opacity, and a few caption
// labels without units. Static: drawn once, only the label texts change. Not clickable.

#include <cmath>
#include <cstdio>

#include "widget_common.h"

namespace esphome::dial_ui {

static constexpr int32_t SCALE_TICK_OUT = 102;   // outer end of a tick, from the centre (the ring's inner edge is 106)
static constexpr int32_t SCALE_TICK_MAJOR = 97;  // inner end of a major tick
static constexpr int32_t SCALE_TICK_MINOR = 99;  // inner end of a minor tick
static constexpr int32_t SCALE_LABEL_R = 88;     // label centre radius (end labels)
static constexpr int32_t SCALE_LABEL_MID_R = 86; // label centre radius (the other labels)
static constexpr size_t SCALE_MAX_TICKS = 5;
static constexpr size_t SCALE_MAX_LABELS = 3;

class ArcScale {
 public:
  // `ticks` evenly spaced over the sweep: 3 (min, middle, max) or 5 (adds the quarters, shorter).
  void create(lv_obj_t *parent, size_t ticks) {
    lv_obj_t *layer = lv_obj_create(parent);
    lv_obj_remove_style_all(layer);
    lv_obj_remove_flag(layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(layer, DIALED_DISPLAY, DIALED_DISPLAY);
    lv_obj_set_pos(layer, 0, 0);
    this->layer_ = layer;
    if (ticks > SCALE_MAX_TICKS)
      ticks = SCALE_MAX_TICKS;
    for (size_t i = 0; i < ticks; i++) {
      float f = ticks > 1 ? static_cast<float>(i) / static_cast<float>(ticks - 1) : 0.0f;
      bool major = ticks <= 3 || i % 2 == 0;
      float cx, cy;
      polar_(f, SCALE_TICK_OUT, &cx, &cy);
      float ix, iy;
      polar_(f, major ? SCALE_TICK_MAJOR : SCALE_TICK_MINOR, &ix, &iy);
      this->pts_[i][0] = {static_cast<lv_value_precise_t>(lroundf(cx)), static_cast<lv_value_precise_t>(lroundf(cy))};
      this->pts_[i][1] = {static_cast<lv_value_precise_t>(lroundf(ix)), static_cast<lv_value_precise_t>(lroundf(iy))};
      lv_obj_t *line = lv_line_create(layer);
      lv_line_set_points(line, this->pts_[i], 2);
      lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_line_width(line, 1, LV_PART_MAIN);
      lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN);
      lv_obj_set_style_line_color(line, DIALED_INK_MUTED, LV_PART_MAIN);
      lv_obj_set_style_line_opa(line, major ? LV_OPA_40 : LV_OPA_30, LV_PART_MAIN);
    }
  }

  // Adds a caption label centred at fraction `frac` (0..1 of the sweep) and `radius` from the centre.
  // Returns its index for set_label().
  size_t add_label(float frac, int32_t radius) {
    size_t i = this->n_labels_ < SCALE_MAX_LABELS ? this->n_labels_++ : SCALE_MAX_LABELS - 1;
    lv_obj_t *l = lv_label_create(this->layer_);
    lv_obj_set_style_text_font(l, DIALED_FONT_CAPTION, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, DIALED_INK_MUTED, LV_PART_MAIN);
    lv_obj_set_style_text_opa(l, LV_OPA_80, LV_PART_MAIN);
    float x, y;
    polar_(frac, radius, &x, &y);
    lv_obj_align(l, LV_ALIGN_CENTER, static_cast<int32_t>(lroundf(x)) - DIALED_DISPLAY / 2,
                 static_cast<int32_t>(lroundf(y)) - DIALED_DISPLAY / 2);
    this->labels_[i] = l;
    return i;
  }

  void set_label(size_t i, const char *text) {
    if (i < this->n_labels_)
      lv_label_set_text(this->labels_[i], text);
  }

  // The three labels of a value scale (min, middle, max), without units: "10", "25", "40".
  void set_range(float lo, float hi) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%g", static_cast<double>(lo));
    this->set_label(0, buf);
    snprintf(buf, sizeof(buf), "%g", static_cast<double>((lo + hi) / 2.0f));
    this->set_label(1, buf);
    snprintf(buf, sizeof(buf), "%g", static_cast<double>(hi));
    this->set_label(2, buf);
  }

  void set_visible(bool visible) {
    if (this->layer_ == nullptr)
      return;
    if (visible)
      lv_obj_remove_flag(this->layer_, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(this->layer_, LV_OBJ_FLAG_HIDDEN);
  }

 protected:
  // Point at `frac` of the 270 degree sweep (from the lower left, clockwise), `radius` from the centre.
  static void polar_(float frac, int32_t radius, float *x, float *y) {
    float a = (static_cast<float>(DIALED_ARC_ROTATION) + frac * static_cast<float>(DIALED_ARC_SWEEP)) *
              static_cast<float>(M_PI) / 180.0f;
    *x = DIALED_DISPLAY / 2.0f + static_cast<float>(radius) * cosf(a);
    *y = DIALED_DISPLAY / 2.0f + static_cast<float>(radius) * sinf(a);
  }

  lv_obj_t *layer_{nullptr};
  lv_obj_t *labels_[SCALE_MAX_LABELS]{};
  size_t n_labels_{0};
  lv_point_precise_t pts_[SCALE_MAX_TICKS][2];
};

}  // namespace esphome::dial_ui
