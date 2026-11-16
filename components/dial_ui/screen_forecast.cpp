#include "screen_forecast.h"

#include <cmath>

#include "esphome/core/time.h"

namespace esphome::dial_ui {

// Geometry from design ScreenForecast: 44 px columns centred on x 120 (five visible).
static constexpr int32_t COL_W = 44;
static constexpr int32_t STRIP_TOP = 72;
static constexpr int32_t STRIP_H = 96;
static constexpr int32_t SIDE_PAD = (DIALED_DISPLAY - COL_W) / 2;  // lets the first and last column centre
// Column content tops, relative to the strip (screen tops 76, 96, 134).
static constexpr int32_t HOUR_Y = 76 - STRIP_TOP;
static constexpr int32_t ICON_Y = 96 - STRIP_TOP;
static constexpr int32_t TEMP_Y = 134 - STRIP_TOP;
static constexpr int32_t BAR_Y = TEMP_Y + 26;
static constexpr int32_t DAY_TOP = 30;
static constexpr int32_t WORD_TOP = 180;

void ScreenForecast::create() {
  this->screen_ = create_screen();
  this->day_ = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK_MUTED, DAY_TOP);
  this->word_ = create_text(this->screen_, DIALED_FONT_LABEL, DIALED_INK_MUTED, WORD_TOP);
  this->status_.create(this->screen_, 112);

  lv_obj_t *strip = lv_obj_create(this->screen_);
  lv_obj_remove_style_all(strip);
  lv_obj_set_size(strip, DIALED_DISPLAY, STRIP_H);
  lv_obj_set_pos(strip, 0, STRIP_TOP);
  lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_left(strip, SIDE_PAD, LV_PART_MAIN);
  lv_obj_set_style_pad_right(strip, SIDE_PAD, LV_PART_MAIN);
  lv_obj_set_scroll_dir(strip, LV_DIR_HOR);
  lv_obj_set_scrollbar_mode(strip, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scroll_snap_x(strip, LV_SCROLL_SNAP_CENTER);
  lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLL_ONE);
  lv_obj_add_event_cb(strip, &ScreenForecast::strip_event_cb_, LV_EVENT_SCROLL_END, this);

  for (size_t i = 0; i < MAX_HOURS; i++) {
    Column &c = this->cols_[i];
    c.box = lv_obj_create(strip);
    lv_obj_remove_style_all(c.box);
    lv_obj_remove_flag(c.box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c.box, LV_OBJ_FLAG_SNAPPABLE);
    lv_obj_set_size(c.box, COL_W, STRIP_H);
    lv_obj_add_event_cb(c.box, &ScreenForecast::column_event_cb_, LV_EVENT_SHORT_CLICKED, this);
    c.hour = create_text(c.box, DIALED_FONT_CAPTION, DIALED_INK_MUTED, HOUR_Y);
    c.icon = create_weather_icon(c.box);
    lv_obj_align(c.icon, LV_ALIGN_TOP_MID, 0, ICON_Y);
    c.temp = create_text(c.box, DIALED_FONT_TITLE, DIALED_INK, TEMP_Y);
    c.bar = lv_obj_create(c.box);
    lv_obj_remove_style_all(c.bar);
    lv_obj_remove_flag(c.bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(c.bar, 24, 2);
    lv_obj_align(c.bar, LV_ALIGN_TOP_MID, 0, BAR_Y);
    lv_obj_set_style_bg_color(c.bar, DIALED_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c.bar, LV_OPA_COVER, LV_PART_MAIN);
  }
  this->strip_ = strip;
  this->set_data(nullptr, 0, 0);
}

void ScreenForecast::set_data(const ForecastHour *rows, size_t n, int64_t now) {
  if (n > MAX_HOURS)
    n = MAX_HOURS;
  this->count_ = n;
  this->now_ = now;
  for (size_t i = 0; i < n; i++)
    this->rows_[i] = rows[i];
  if (n == 0) {  // nothing loaded yet: OFFLINE in the middle, no strip
    lv_obj_add_flag(this->strip_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(this->day_, "Forecast");
    lv_label_set_text(this->word_, "");
    this->status_.set(Status::OFFLINE);
    this->status_.set_visible(true);
    return;
  }
  this->status_.set_visible(false);
  lv_obj_remove_flag(this->strip_, LV_OBJ_FLAG_HIDDEN);
  for (size_t i = 0; i < MAX_HOURS; i++) {
    Column &c = this->cols_[i];
    if (i >= n) {
      lv_obj_add_flag(c.box, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(c.box, LV_OBJ_FLAG_HIDDEN);
    if (i == 0) {
      lv_label_set_text(c.hour, "Now");
    } else {
      ESPTime t = ESPTime::from_epoch_local(static_cast<time_t>(rows[i].epoch));
      lv_label_set_text_fmt(c.hour, "%dh", t.hour);
    }
    ConditionInfo ci;
    if (condition_info(rows[i].condition, &ci)) {
      lv_image_set_src(c.icon, weather_icon_src(ci.icon, false));
      lv_obj_remove_flag(c.icon, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(c.icon, LV_OBJ_FLAG_HIDDEN);
    }
    if (std::isnan(rows[i].temp))
      lv_label_set_text(c.temp, "--");
    else
      lv_label_set_text_fmt(c.temp, "%d\xC2\xB0", static_cast<int>(std::lround(rows[i].temp)));
  }
  this->focus(this->focus_ < n ? this->focus_ : 0);
}

void ScreenForecast::focus(size_t i, bool animate) {
  if (this->count_ == 0)
    return;
  if (i >= this->count_)
    i = this->count_ - 1;
  lv_obj_update_layout(this->strip_);
  lv_obj_scroll_to_x(this->strip_, static_cast<int32_t>(i) * COL_W, animate ? LV_ANIM_ON : LV_ANIM_OFF);
  this->apply_focus_(i);  // focus styling follows at once; the strip scrolls under it
}

void ScreenForecast::step(int delta) {
  int i = static_cast<int>(this->focus_) + delta;
  if (i < 0 || i >= static_cast<int>(this->count_))
    return;
  this->focus(static_cast<size_t>(i), true);
}

void ScreenForecast::apply_focus_(size_t f) {
  this->focus_ = f;
  for (size_t i = 0; i < this->count_; i++) {
    Column &c = this->cols_[i];
    size_t d = i > f ? i - f : f - i;
    // Focus at full strength; neighbours `opa-dim`, the next ones `opa-disabled`.
    lv_obj_set_style_opa(c.box, d == 0 ? LV_OPA_COVER : (d == 1 ? DIALED_OPA_DIM : DIALED_OPA_DISABLED),
                         LV_PART_MAIN);
    lv_obj_set_style_text_color(c.hour, d == 0 ? DIALED_INK : DIALED_INK_MUTED, LV_PART_MAIN);
    if (d == 0)
      lv_obj_remove_flag(c.bar, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(c.bar, LV_OBJ_FLAG_HIDDEN);
  }
  // Day of the focused hour ("Today" / "Tomorrow", switches at local midnight) and its condition.
  ESPTime now = ESPTime::from_epoch_local(static_cast<time_t>(this->now_));
  ESPTime t = ESPTime::from_epoch_local(static_cast<time_t>(this->rows_[f].epoch));
  bool today = t.year == now.year && t.day_of_year == now.day_of_year;
  lv_label_set_text(this->day_, today ? "Today" : "Tomorrow");
  ConditionInfo ci;
  lv_label_set_text(this->word_, condition_info(this->rows_[f].condition, &ci) ? ci.word : "");
}

void ScreenForecast::strip_event_cb_(lv_event_t *e) {
  auto *self = static_cast<ScreenForecast *>(lv_event_get_user_data(e));
  if (self->count_ == 0)
    return;
  // After a drag the strip snaps on a column centre: that column is the focus.
  // An encoder step that interrupts a running scroll also ends here: the last step wins.
  int32_t x = lv_obj_get_scroll_x(self->strip_);
  int32_t i = (x + COL_W / 2) / COL_W;
  if (i < 0)
    i = 0;
  if (i >= static_cast<int32_t>(self->count_))
    i = static_cast<int32_t>(self->count_) - 1;
  if (static_cast<size_t>(i) != self->focus_)
    self->apply_focus_(static_cast<size_t>(i));
}

void ScreenForecast::column_event_cb_(lv_event_t *e) {
  auto *self = static_cast<ScreenForecast *>(lv_event_get_user_data(e));
  lv_obj_t *box = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
  for (size_t i = 0; i < self->count_; i++)
    if (self->cols_[i].box == box)
      self->focus(i, true);  // a tap centres the column
}

}  // namespace esphome::dial_ui
