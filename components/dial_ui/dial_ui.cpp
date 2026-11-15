#include "dial_ui.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome::dial_ui {

static const char *const TAG = "dial_ui";
static constexpr uint32_t REFRESH_MS = 1000;      // design: update readings at 1 Hz
static constexpr uint32_t SETTINGS_HOLD_MS = 600;  // spec: 600 ms touch hold opens Settings
static constexpr int32_t HOLD_SLOP_PX = 12;        // a hold that moves further is a drag, not a hold

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
    this->readings_per_room_.push_back(this->rooms_[r].readings.size());
    this->room_screens_[r].create(this->rooms_[r]);
    this->room_screens_[r].set_dots(this->rooms_.size(), r);
    this->attach_events_(this->room_screens_[r].screen());
  }
  for (auto &scr : this->sensor_screens_) {
    scr.create();
    this->attach_events_(scr.screen());
  }
  this->settings_screen_.create();
  this->attach_events_(this->settings_screen_.screen());

  this->hold_timer_ = lv_timer_create(&DialUi::hold_timer_cb_, SETTINGS_HOLD_MS, this);
  lv_timer_pause(this->hold_timer_);

  if (this->rooms_.empty()) {
    ESP_LOGW(TAG, "No rooms configured");
    return;
  }
  this->online_last_ = this->online_();
  this->refresh_();
  this->page_ = Page{PageKind::SETTINGS, 0, 0};  // so the first show_() places the dots directly
  this->show_(Page{PageKind::ROOM, 0, 0}, Slide::NONE);  // boots on the first room
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
      ScreenSensor *scr = this->visible_sensor_();
      if (this->page_.kind == PageKind::SENSOR && scr->is_bound_to(ri, i))
        scr->update(room, online, true);
    }
  }
}

// ---- Navigation (spec.md "Navigation rules") ----

void DialUi::nav_(NavAction action) {
  NavShape shape{this->rooms_.size(), this->readings_per_room_.data()};
  NavResult r = nav_apply(this->page_, action, shape, &this->before_settings_);
  if (r.changed)
    this->show_(r.page, r.slide);
}

void DialUi::open_metric_(size_t room, size_t index) {
  if (room < this->rooms_.size() && index < this->rooms_[room].readings.size())
    this->show_(Page{PageKind::SENSOR, room, index}, Slide::UP);
}

void DialUi::show_(const Page &page, Slide slide) {
  this->cancel_hold_();
  Page prev = this->page_;
  this->page_ = page;
  switch (page.kind) {
    case PageKind::ROOM: {
      ScreenRoom &scr = this->room_screens_[page.room];
      // Dots glide when moving along the carousel; otherwise they are placed directly.
      if (prev.kind == PageKind::ROOM && prev.room != page.room)
        scr.animate_dots(this->rooms_.size(), prev.room, page.room);
      else
        scr.set_dots(this->rooms_.size(), page.room);
      this->load_(scr.screen(), slide);
      break;
    }
    case PageKind::SENSOR: {
      this->sensor_active_ ^= 1;
      ScreenSensor *scr = this->visible_sensor_();
      bool sibling = prev.kind == PageKind::SENSOR && prev.room == page.room;
      scr->bind(this->rooms_[page.room], page.room, page.index, this->online_last_,
                sibling ? prev.index : SIZE_MAX);
      this->load_(scr->screen(), slide);
      break;
    }
    case PageKind::SETTINGS:
      this->load_(this->settings_screen_.screen(), slide);
      break;
  }
}

void DialUi::load_(lv_obj_t *screen, Slide slide) {
  // No slide animation (user, 2026-11-15): a 200 ms full-screen slide redraws the whole
  // 240x240 panel every frame over SPI and felt laggy on the device. `slide` is kept so
  // the direction is still known if a cheaper transition is tried later.
  (void) slide;
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
  static const lv_event_code_t CODES[] = {LV_EVENT_KEY,     LV_EVENT_SHORT_CLICKED, LV_EVENT_LONG_PRESSED,
                                          LV_EVENT_GESTURE, LV_EVENT_PRESSED,       LV_EVENT_RELEASED,
                                          LV_EVENT_PRESS_LOST};
  for (lv_event_code_t c : CODES)
    lv_obj_add_event_cb(screen, &DialUi::event_cb_, c, this);
}

void DialUi::event_cb_(lv_event_t *e) { static_cast<DialUi *>(lv_event_get_user_data(e))->on_event_(e); }

void DialUi::on_event_(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
  lv_indev_t *indev = lv_indev_active();
  bool from_encoder = indev != nullptr && lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER;
  bool from_touch = indev != nullptr && lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER;

  switch (code) {
    case LV_EVENT_KEY: {  // encoder turn
      uint32_t key = lv_event_get_key(e);
      if (key == LV_KEY_RIGHT)
        this->nav_(NavAction::NEXT);
      else if (key == LV_KEY_LEFT)
        this->nav_(NavAction::PREV);
      break;
    }
    case LV_EVENT_SHORT_CLICKED:
      if (from_encoder) {
        this->nav_(NavAction::OPEN);  // press = deeper
      } else if (from_touch && this->page_.kind == PageKind::ROOM) {
        int slot = this->room_screens_[this->page_.room].slot_index(target);
        if (slot >= 0)
          this->open_metric_(this->page_.room, static_cast<size_t>(slot));  // tap on a reading
      }
      break;
    case LV_EVENT_LONG_PRESSED:
      // Encoder long press (600 ms) = back. A touch hold is handled by the hold timer instead.
      if (from_encoder)
        this->nav_(NavAction::BACK);
      break;
    case LV_EVENT_GESTURE: {
      this->cancel_hold_();
      lv_dir_t dir = lv_indev_get_gesture_dir(indev);
      // Ignore the rest of this touch, so the release is not also read as a tap.
      lv_indev_wait_release(indev);
      Swipe swipe;
      if (dir == LV_DIR_LEFT)
        swipe = Swipe::LEFT;
      else if (dir == LV_DIR_RIGHT)
        swipe = Swipe::RIGHT;
      else if (dir == LV_DIR_TOP)
        swipe = Swipe::UP;
      else if (dir == LV_DIR_BOTTOM)
        swipe = Swipe::DOWN;
      else
        break;
      NavAction action;
      if (swipe_action(this->page_.kind, swipe, &action))
        this->nav_(action);
      break;
    }
    case LV_EVENT_PRESSED:
      if (from_touch && this->page_.kind == PageKind::ROOM) {
        this->hold_indev_ = indev;
        lv_indev_get_point(indev, &this->hold_point_);
        lv_timer_reset(this->hold_timer_);
        lv_timer_resume(this->hold_timer_);
      }
      break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
      if (from_touch)
        this->cancel_hold_();
      break;
    default:
      break;
  }
}

void DialUi::cancel_hold_() {
  if (this->hold_timer_ != nullptr)
    lv_timer_pause(this->hold_timer_);
  this->hold_indev_ = nullptr;
}

void DialUi::hold_timer_cb_(lv_timer_t *t) {
  auto *self = static_cast<DialUi *>(lv_timer_get_user_data(t));
  lv_indev_t *indev = self->hold_indev_;
  self->cancel_hold_();
  if (indev == nullptr || self->page_.kind != PageKind::ROOM)
    return;
  lv_point_t p;
  lv_indev_get_point(indev, &p);
  int32_t dx = p.x - self->hold_point_.x, dy = p.y - self->hold_point_.y;
  if (dx * dx + dy * dy > HOLD_SLOP_PX * HOLD_SLOP_PX)
    return;  // the finger moved: a slow drag, not a hold
  lv_indev_wait_release(indev);  // the release must not act on the Settings page
  self->nav_(NavAction::BACK);   // at the top level, back opens Settings
}

void DialUi::dump_config() {
  ESP_LOGCONFIG(TAG, "Dial UI: %u room(s)", static_cast<unsigned>(this->rooms_.size()));
  for (const auto &room : this->rooms_)
    ESP_LOGCONFIG(TAG, "  %s (%s): %u reading(s)", room.name.c_str(), room.id.c_str(),
                  static_cast<unsigned>(room.readings.size()));
}

}  // namespace esphome::dial_ui
