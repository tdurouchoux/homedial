#include "screen_room.h"

namespace esphome::dial_ui {

static constexpr int32_t NAME_TOP = 30;
static constexpr int32_t SLOT_W = 84;    // >= 82 (design touch target)
static constexpr int32_t SLOT_H = 62;    // >= 58
static constexpr int32_t LABEL_H = 18;   // label line above the value

struct SlotPos {
  int32_t cx;
  int32_t top;
};

// Layout by reading count (design ScreenRoom). Tops are the label line's top.
static void layout(size_t count, SlotPos *pos, const lv_font_t **font) {
  switch (count) {
    case 1:
      pos[0] = {120, 89};
      *font = DIALED_FONT_VALUE_L;
      break;
    case 2:
      pos[0] = {70, 89};
      pos[1] = {170, 89};
      *font = DIALED_FONT_VALUE_L;
      break;
    case 3:
      pos[0] = {120, 60};
      pos[1] = {78, 126};
      pos[2] = {162, 126};
      *font = DIALED_FONT_VALUE_M;
      break;
    default:
      pos[0] = {78, 60};
      pos[1] = {162, 60};
      pos[2] = {78, 126};
      pos[3] = {162, 126};
      *font = DIALED_FONT_VALUE_M;
      break;
  }
}

void ScreenRoom::create(const Room &room) {
  this->screen_ = create_screen();
  lv_obj_t *name = create_location_label(this->screen_, DIALED_FONT_LABEL, NAME_TOP);
  lv_label_set_text(name, room.name.c_str());

  this->slot_count_ = room.readings.size() > METRIC_COUNT ? METRIC_COUNT : room.readings.size();
  SlotPos pos[METRIC_COUNT];
  const lv_font_t *value_font = DIALED_FONT_VALUE_M;
  layout(this->slot_count_, pos, &value_font);

  for (size_t i = 0; i < this->slot_count_; i++) {
    Slot &s = this->slots_[i];
    s.box = lv_obj_create(this->screen_);
    lv_obj_remove_style_all(s.box);
    lv_obj_remove_flag(s.box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s.box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s.box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // value-l text may be wider than the box
    lv_obj_add_flag(s.box, LV_OBJ_FLAG_EVENT_BUBBLE);      // taps reach the screen handler
    lv_obj_set_size(s.box, SLOT_W, SLOT_H);
    lv_obj_set_pos(s.box, pos[i].cx - SLOT_W / 2, pos[i].top - 4);
    lv_obj_set_style_radius(s.box, DIALED_RADIUS_MD, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s.box, DIALED_SURFACE_2, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(s.box, LV_OPA_COVER, LV_STATE_PRESSED);

    s.label = create_text(s.box, DIALED_FONT_LABEL, DIALED_INK_MUTED, 4);
    lv_label_set_text(s.label, metric_info(room.readings[i].metric).short_label);
    s.value = create_text(s.box, value_font, DIALED_INK_MUTED, 4 + LABEL_H);
    lv_label_set_text(s.value, "--");
  }
  this->dots_.create(this->screen_);
}

void ScreenRoom::update_reading(const Room &room, size_t index, bool online) {
  if (index >= this->slot_count_)
    return;
  const Reading &r = room.readings[index];
  char buf[16];
  format_value(buf, sizeof(buf), r.value, r.metric, online);
  Status st = compute_status(r.value, r.thresholds, online);
  lv_label_set_text(this->slots_[index].value, buf);
  lv_obj_set_style_text_color(this->slots_[index].value, value_color(st), LV_PART_MAIN);
}

int ScreenRoom::slot_index(lv_obj_t *obj) const {
  for (size_t i = 0; i < this->slot_count_; i++)
    if (this->slots_[i].box == obj)
      return static_cast<int>(i);
  return -1;
}

}  // namespace esphome::dial_ui
