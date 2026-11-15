#include "dial_ui.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome::dial_ui {

static const char *const TAG = "dial_ui";
static constexpr uint32_t REFRESH_MS = 1000;  // design: update readings at 1 Hz

void DialUi::add_room(const char *id, const char *name) {
  Room r;
  r.id = id;
  r.name = name;
  this->rooms_.push_back(std::move(r));
}

void DialUi::add_reading(Metric metric, sensor::Sensor *sensor, Thresholds thresholds) {
  if (this->rooms_.empty())
    return;
  size_t room = this->rooms_.size() - 1;
  Reading r;
  r.metric = metric;
  r.thresholds = thresholds;
  this->rooms_[room].readings.push_back(r);
  size_t index = this->rooms_[room].readings.size() - 1;
  if (sensor != nullptr) {
    // Inbound callbacks only write the model; loop() redraws (contract.md §1).
    sensor->add_on_state_callback([this, room, index](float x) { this->set_value_(room, index, x); });
  }
}

void DialUi::set_reading(const char *room_id, const char *metric, float value) {
  Metric m;
  if (!parse_metric(metric, &m)) {
    ESP_LOGW(TAG, "set_reading: unknown metric '%s'", metric);
    return;
  }
  for (size_t r = 0; r < this->rooms_.size(); r++) {
    if (this->rooms_[r].id != room_id)
      continue;
    int i = this->rooms_[r].index_of(m);
    if (i < 0)
      break;
    this->set_value_(r, static_cast<size_t>(i), value);
    return;
  }
  ESP_LOGW(TAG, "set_reading: no %s reading in room '%s'", metric, room_id);
}

void DialUi::set_value_(size_t room, size_t index, float value) {
  Reading &r = this->rooms_[room].readings[index];
  r.value = value;
  r.dirty = true;
}

bool DialUi::online_() const {
#ifdef USE_API
  // Home Assistant is connected (a client subscribed to HA states), contract.md §4.
  return api::global_api_server != nullptr && api::global_api_server->is_connected_with_state_subscription();
#else
  return true;
#endif
}

void DialUi::setup() {
  this->room_screens_.resize(this->rooms_.size());
  for (size_t r = 0; r < this->rooms_.size(); r++) {
    this->room_screens_[r].create(this->rooms_[r]);
    this->attach_events_(this->room_screens_[r].screen());
  }
  this->sensor_screen_.create();
  this->attach_events_(this->sensor_screen_.screen());

  if (this->rooms_.empty()) {
    ESP_LOGW(TAG, "No rooms configured");
    return;
  }
  this->online_last_ = this->online_();
  this->refresh_();
  this->show_room_(0);
}

void DialUi::loop() {
  uint32_t now = millis();
  if (now - this->last_refresh_ms_ < REFRESH_MS)
    return;
  this->last_refresh_ms_ = now;
  bool online = this->online_();
  if (online != this->online_last_) {
    this->online_last_ = online;
    ESP_LOGD(TAG, "Home Assistant %s", online ? "connected" : "disconnected");
    for (auto &room : this->rooms_)
      for (auto &r : room.readings)
        r.dirty = true;
  }
  this->refresh_();
}

void DialUi::refresh_() {
  bool online = this->online_last_;
  for (size_t ri = 0; ri < this->rooms_.size(); ri++) {
    Room &room = this->rooms_[ri];
    for (size_t i = 0; i < room.readings.size(); i++) {
      if (!room.readings[i].dirty)
        continue;
      room.readings[i].dirty = false;
      this->room_screens_[ri].update_reading(room, i, online);
      if (this->sensor_screen_.is_bound_to(ri, i))
        this->sensor_screen_.update(room, online, this->level_ == Level::SENSOR);
    }
  }
}

// ---- Navigation (M2 subset of spec.md; gestures, slides and Settings come in M3) ----

void DialUi::show_room_(size_t room) {
  this->level_ = Level::ROOM;
  this->room_ = room;
  this->room_screens_[room].set_dots(this->rooms_.size(), room);
  this->load_(this->room_screens_[room].screen());
}

void DialUi::show_sensor_(size_t room, size_t index) {
  this->level_ = Level::SENSOR;
  this->room_ = room;
  this->sensor_screen_.bind(this->rooms_[room], room, index, this->online_last_);
  this->load_(this->sensor_screen_.screen());
}

void DialUi::load_(lv_obj_t *screen) {
  lv_screen_load(screen);
  // The screen itself is the only object in the encoder group, in edit mode, so turning
  // sends LV_KEY_LEFT/RIGHT to it and the push sends SHORT_CLICKED / LONG_PRESSED
  // (lvgl src/indev/lv_indev.c, indev_encoder_proc).
  lv_group_t *g = lv_group_get_default();
  if (g == nullptr)
    return;
  lv_group_remove_all_objs(g);
  lv_group_add_obj(g, screen);
  lv_group_focus_obj(screen);
  lv_group_set_editing(g, true);
}

void DialUi::attach_events_(lv_obj_t *screen) {
  lv_obj_add_event_cb(screen, &DialUi::event_cb_, LV_EVENT_KEY, this);
  lv_obj_add_event_cb(screen, &DialUi::event_cb_, LV_EVENT_SHORT_CLICKED, this);
  lv_obj_add_event_cb(screen, &DialUi::event_cb_, LV_EVENT_LONG_PRESSED, this);
}

void DialUi::event_cb_(lv_event_t *e) { static_cast<DialUi *>(lv_event_get_user_data(e))->on_event_(e); }

void DialUi::on_event_(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
  lv_indev_t *indev = lv_indev_active();
  bool from_encoder = indev != nullptr && lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER;

  if (this->level_ == Level::ROOM) {
    ScreenRoom &scr = this->room_screens_[this->room_];
    if (code == LV_EVENT_KEY) {
      uint32_t key = lv_event_get_key(e);
      if (key == LV_KEY_RIGHT && this->room_ + 1 < this->rooms_.size())
        this->show_room_(this->room_ + 1);
      else if (key == LV_KEY_LEFT && this->room_ > 0)
        this->show_room_(this->room_ - 1);
    } else if (code == LV_EVENT_SHORT_CLICKED) {
      int slot = scr.slot_index(target);
      if (slot >= 0)
        this->show_sensor_(this->room_, static_cast<size_t>(slot));  // tap on a reading
      else if (from_encoder && !this->rooms_[this->room_].readings.empty())
        this->show_sensor_(this->room_, 0);  // press opens the first metric
    }
    return;
  }

  // Level::SENSOR
  size_t index = this->sensor_screen_.index();
  const Room &room = this->rooms_[this->room_];
  if (code == LV_EVENT_KEY) {
    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_RIGHT && index + 1 < room.readings.size())
      this->show_sensor_(this->room_, index + 1);
    else if (key == LV_KEY_LEFT && index > 0)
      this->show_sensor_(this->room_, index - 1);
  } else if (code == LV_EVENT_LONG_PRESSED && from_encoder) {
    this->show_room_(this->room_);  // back
  }
}

void DialUi::dump_config() {
  ESP_LOGCONFIG(TAG, "Dial UI: %u room(s)", static_cast<unsigned>(this->rooms_.size()));
  for (const auto &room : this->rooms_)
    ESP_LOGCONFIG(TAG, "  %s (%s): %u reading(s)", room.name.c_str(), room.id.c_str(),
                  static_cast<unsigned>(room.readings.size()));
}

}  // namespace esphome::dial_ui
