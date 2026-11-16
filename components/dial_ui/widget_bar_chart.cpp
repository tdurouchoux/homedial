#include "widget_bar_chart.h"

#include "model_energy.h"

namespace esphome::dial_ui {

static constexpr int32_t BASELINE_Y = 72;          // 1 px `track` line
static constexpr int32_t BOTTOM = BASELINE_Y - 1;  // bars stand on the line
static constexpr int32_t PLOT_H = BOTTOM;          // tallest bar
static constexpr int32_t BAR_W = 4;                // 62 % of a 6.67 px slot
static constexpr int32_t STUB_H = 2;

static lv_obj_t *make_rect(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
  return o;
}

void BarChart::create(lv_obj_t *parent, int32_t top) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(box, W, H);
  lv_obj_align(box, LV_ALIGN_TOP_MID, 0, top);

  lv_obj_t *line = make_rect(box);
  lv_obj_set_size(line, W, 1);
  lv_obj_set_pos(line, 0, BASELINE_Y);
  lv_obj_set_style_bg_color(line, DIALED_TRACK, LV_PART_MAIN);

  for (size_t i = 0; i < N; i++) {
    int32_t x0 = static_cast<int32_t>(i) * W / static_cast<int32_t>(N);
    int32_t x1 = static_cast<int32_t>(i + 1) * W / static_cast<int32_t>(N);
    lv_obj_t *b = make_rect(box);
    lv_obj_set_width(b, BAR_W);
    lv_obj_set_x(b, x0 + (x1 - x0 - BAR_W) / 2);
    lv_obj_set_style_bg_color(b, DIALED_TRACK, LV_PART_MAIN);
    lv_obj_set_style_radius(b, 1, LV_PART_MAIN);
    lv_obj_set_height(b, STUB_H);
    lv_obj_set_y(b, BOTTOM - STUB_H);
    this->bars_[i] = b;
    this->height_[i] = STUB_H;
    this->kind_[i] = 3;
  }
  this->box_ = box;
}

void BarChart::set_data(const float *values, size_t current, float axis_max) {
  for (size_t i = 0; i < N; i++) {
    float v = values[i];
    int32_t h;
    uint8_t kind;
    if (std::isnan(v)) {
      h = STUB_H;
      kind = 3;
    } else {
      h = bar_px(v, axis_max, PLOT_H);
      kind = i == current ? 2 : 1;
    }
    lv_obj_t *b = this->bars_[i];
    if (h != this->height_[i]) {
      lv_obj_set_height(b, h);
      lv_obj_set_y(b, BOTTOM - h);
      lv_obj_set_style_radius(b, h >= 4 ? 2 : 1, LV_PART_MAIN);
      this->height_[i] = h;
    }
    if (kind != this->kind_[i]) {
      lv_obj_set_style_bg_color(b, kind == 2 ? DIALED_ACCENT : (kind == 1 ? DIALED_LINE : DIALED_TRACK),
                                LV_PART_MAIN);
      this->kind_[i] = kind;
    }
  }
}

}  // namespace esphome::dial_ui
