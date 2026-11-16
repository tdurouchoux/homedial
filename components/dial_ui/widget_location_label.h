#pragma once
// LocationLabel (design/components/LocationLabel, RoomIcon): the room name, one centred line, with
// an optional 16 px room icon in front (6 px gap); icon and name centre together as one line.
// The whole line is at most 120 px wide; a long name ends in an ellipsis.

#include <string>

#include "widget_common.h"

extern "C" {
// tools/gen_icons.js -> room_icons.c (A8 images drawn in the recolour colour).
extern const lv_image_dsc_t dialed_room_living_room_16, dialed_room_bedroom_16, dialed_room_kitchen_16,
    dialed_room_bathroom_16, dialed_room_office_16, dialed_room_garage_16, dialed_room_garden_16,
    dialed_room_other_16;
}

namespace esphome::dial_ui {

static constexpr int32_t LOCATION_LABEL_W = 120;
static constexpr int32_t ROOM_ICON_GAP = 6;

// Room icon id: 0 = none, 1..8 = living room, bedroom, kitchen, bathroom, office, garage, garden,
// other (design RoomIcon order).
inline const lv_image_dsc_t *room_icon_src(uint8_t id) {
  static const lv_image_dsc_t *const ICONS[] = {
      &dialed_room_living_room_16, &dialed_room_bedroom_16, &dialed_room_kitchen_16, &dialed_room_bathroom_16,
      &dialed_room_office_16,      &dialed_room_garage_16,  &dialed_room_garden_16,  &dialed_room_other_16};
  return (id >= 1 && id <= 8) ? ICONS[id - 1] : nullptr;
}

class LocationLabel {
 public:
  void create(lv_obj_t *parent, const lv_font_t *font, int32_t top) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, LOCATION_LABEL_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, ROOM_ICON_GAP, LV_PART_MAIN);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, top);
    this->icon_ = lv_image_create(row);
    lv_obj_remove_flag(this->icon_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_image_recolor(this->icon_, DIALED_INK, LV_PART_MAIN);
    lv_obj_set_style_image_recolor_opa(this->icon_, LV_OPA_COVER, LV_PART_MAIN);
    this->name_ = lv_label_create(row);
    lv_obj_set_style_text_font(this->name_, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->name_, DIALED_INK, LV_PART_MAIN);
    lv_label_set_long_mode(this->name_, LV_LABEL_LONG_DOT);
    this->row_ = row;
    this->set("", 0);
  }

  void set(const std::string &name, uint8_t icon) {
    const lv_image_dsc_t *src = room_icon_src(icon);
    if (src != nullptr) {
      lv_image_set_src(this->icon_, src);
      lv_obj_remove_flag(this->icon_, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(this->icon_, LV_OBJ_FLAG_HIDDEN);  // no icon: the name is centred alone
    }
    // The name takes its own width, up to what is left of the 120 px line; longer ends in "...".
    int32_t max_w = LOCATION_LABEL_W - (src != nullptr ? DIALED_ICON_S + ROOM_ICON_GAP : 0);
    lv_label_set_text(this->name_, name.c_str());
    lv_obj_set_width(this->name_, LV_SIZE_CONTENT);
    lv_obj_update_layout(this->name_);
    if (lv_obj_get_width(this->name_) > max_w)
      lv_obj_set_width(this->name_, max_w);
  }

  lv_obj_t *obj() const { return this->row_; }

 protected:
  lv_obj_t *row_{nullptr};
  lv_obj_t *icon_{nullptr};
  lv_obj_t *name_{nullptr};
};

}  // namespace esphome::dial_ui
