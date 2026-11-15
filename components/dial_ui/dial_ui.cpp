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
static constexpr uint32_t SAMPLE_MS = 60000;       // history: one sample a minute (contract.md §5)
static constexpr uint32_t SCRUB_HOLD_MS = 250;     // design: hold 250 ms on the chart starts a scrub
static constexpr uint32_t SCRUB_KEEP_MS = 2000;    // design: the readout stays 2 s after release
// Arrival nudges (spec.md): content arrives moving the way the finger went.
static constexpr int32_t NUDGE_VALUE_PX = 12;      // Sensor hero value (vertical)
static constexpr int32_t NUDGE_ROOM_PX = 8;        // Room readings (vertical)
static constexpr int32_t NUDGE_SETTINGS_PX = 16;   // Settings content (vertical)
static constexpr int32_t NUDGE_SIDE_PX = 12;       // sideways: room to room, metric to metric

// Swipe up (Slide::UP) moves content up, so it arrives from below (+dy); down arrives from above.
static int32_t arrival_dy(Slide slide, int32_t px) {
  if (slide == Slide::UP)
    return px;
  if (slide == Slide::DOWN)
    return -px;
  return 0;
}
// Swipe left / turn clockwise (Slide::LEFT, next) moves content left: it arrives from the right.
static int32_t arrival_dx(Slide slide, int32_t px) {
  if (slide == Slide::LEFT)
    return px;
  if (slide == Slide::RIGHT)
    return -px;
  return 0;
}

void DialUi::add_room(const char *id, const char *name) {
  Room r;
  r.id = id;
  r.name = name;
  this->rooms_.push_back(std::move(r));
}

void DialUi::add_reading(Metric metric, sensor::Sensor *sensor, Thresholds thresholds, bool history) {
  if (this->rooms_.empty())
    return;
  size_t room = this->rooms_.size() - 1;
  Reading r;
  r.metric = metric;
  r.thresholds = thresholds;
  r.has_history = history;
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
    uint8_t mask = 0;
    for (size_t i = 0; i < this->rooms_[r].readings.size(); i++)
      if (this->rooms_[r].readings[i].has_history)
        mask |= static_cast<uint8_t>(1u << i);
    this->history_mask_.push_back(mask);
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
  this->history_screen_.create();
  this->attach_events_(this->history_screen_.screen());
  lv_obj_t *chart = this->history_screen_.chart().obj();
  for (lv_event_code_t c : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
    lv_obj_add_event_cb(chart, &DialUi::chart_event_cb_, c, this);
  this->scrub_hold_timer_ = lv_timer_create(&DialUi::scrub_hold_cb_, SCRUB_HOLD_MS, this);
  lv_timer_pause(this->scrub_hold_timer_);
  this->scrub_revert_timer_ = lv_timer_create(&DialUi::scrub_revert_cb_, SCRUB_KEEP_MS, this);
  lv_timer_pause(this->scrub_revert_timer_);

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
  if (!this->sampled_once_ || now - this->last_sample_ms_ >= SAMPLE_MS)
    this->sample_history_();
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

// Once a minute: each history reading's current value (from memory, not a request to HA) goes
// into its hourly mean; hours also roll over while offline, so gaps show as missing.
void DialUi::sample_history_() {
  if (this->time_ == nullptr)
    return;
  int64_t epoch = static_cast<int64_t>(this->time_->timestamp_now());
  if (epoch < HourlyHistory::MIN_VALID_EPOCH)
    return;  // clock not set yet: nothing is recorded (try again next loop)
  this->sampled_once_ = true;
  this->last_sample_ms_ = millis();
  bool online = this->online_last_;
  for (auto &room : this->rooms_) {
    for (auto &r : room.readings) {
      if (!r.has_history)
        continue;
      if (online)
        r.history.add(epoch, r.value);  // NaN (sensor unavailable) is ignored
      else
        r.history.advance(epoch);
    }
  }
  if (this->page_.kind == PageKind::HISTORY)
    this->history_screen_.update(this->rooms_[this->page_.room], online);
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
      if (this->page_.kind == PageKind::HISTORY && this->page_.room == ri && this->history_screen_.is_bound_to(ri, i))
        this->history_screen_.update(room, online);
    }
  }
}

// ---- Navigation (spec.md "Navigation rules") ----

void DialUi::nav_(NavAction action) {
  NavShape shape{this->rooms_.size(), this->readings_per_room_.data(), this->history_mask_.data()};
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
  this->end_scrub_();
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
      scr.nudge(arrival_dx(slide, NUDGE_SIDE_PX), arrival_dy(slide, NUDGE_ROOM_PX));
      this->load_(scr.screen(), slide);
      break;
    }
    case PageKind::SENSOR: {
      this->sensor_active_ ^= 1;
      ScreenSensor *scr = this->visible_sensor_();
      bool sibling = prev.kind == PageKind::SENSOR && prev.room == page.room;
      scr->bind(this->rooms_[page.room], page.room, page.index, this->online_last_,
                sibling ? prev.index : SIZE_MAX);
      scr->nudge(arrival_dx(slide, NUDGE_SIDE_PX), arrival_dy(slide, NUDGE_VALUE_PX));
      this->load_(scr->screen(), slide);
      break;
    }
    case PageKind::HISTORY:
      this->history_screen_.bind(this->rooms_[page.room], page.room, page.index, this->online_last_);
      this->history_screen_.nudge(arrival_dy(slide, NUDGE_VALUE_PX));
      this->load_(this->history_screen_.screen(), slide);
      break;
    case PageKind::SETTINGS:
      this->settings_screen_.nudge(arrival_dy(slide, NUDGE_SETTINGS_PX));
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

  if (this->page_.kind == PageKind::HISTORY && this->history_event_(e, code, from_encoder))
    return;

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
      if (this->scrub_hold_timer_ != nullptr)
        lv_timer_pause(this->scrub_hold_timer_);
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

// ---- History scrub (design ScreenHistory; spec.md "Level 2 interaction pages") ----

// Returns true when the event was consumed by the History page.
bool DialUi::history_event_(lv_event_t *e, lv_event_code_t code, bool from_encoder) {
  if (code == LV_EVENT_KEY) {  // turn never changes page here; while scrubbing it moves the cursor
    if (this->scrub_encoder_) {
      int i = this->history_screen_.scrub_index();
      uint32_t key = lv_event_get_key(e);
      if (key == LV_KEY_RIGHT && i + 1 < static_cast<int>(HourlyHistory::POINTS))
        this->scrub_to_(i + 1);
      else if (key == LV_KEY_LEFT && i > 0)
        this->scrub_to_(i - 1);
    }
    return true;
  }
  if (code == LV_EVENT_SHORT_CLICKED && from_encoder) {
    if (this->scrub_encoder_) {
      this->end_scrub_();  // press ends the scrub
    } else {
      const Reading &r = this->rooms_[this->page_.room].readings[this->page_.index];
      int latest = r.history.latest_index();
      this->scrub_encoder_ = true;  // press starts the scrub on the latest sample
      this->scrub_to_(latest >= 0 ? latest : static_cast<int>(HourlyHistory::POINTS) - 1);
    }
    return true;
  }
  if (code == LV_EVENT_LONG_PRESSED && from_encoder && this->scrub_encoder_) {
    this->end_scrub_();  // long press ends a scrub instead of going back
    return true;
  }
  if (code == LV_EVENT_GESTURE && this->scrub_touch_)
    return true;  // a drag while scrubbing is not a swipe
  return false;
}

void DialUi::chart_event_cb_(lv_event_t *e) {
  static_cast<DialUi *>(lv_event_get_user_data(e))->on_chart_event_(e);
}

void DialUi::on_chart_event_(lv_event_t *e) {
  if (this->page_.kind != PageKind::HISTORY)
    return;
  lv_indev_t *indev = lv_indev_active();
  if (indev == nullptr || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER)
    return;
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      // A hold of 250 ms starts the scrub; a quick drag before that stays a swipe.
      this->scrub_indev_ = indev;
      lv_indev_get_point(indev, &this->scrub_point_);
      lv_timer_reset(this->scrub_hold_timer_);
      lv_timer_resume(this->scrub_hold_timer_);
      break;
    case LV_EVENT_PRESSING:
      if (this->scrub_touch_) {
        int i = this->history_screen_.chart().pressed_index();
        if (i >= 0 && i != this->history_screen_.scrub_index())
          this->scrub_to_(i);
      }
      break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
      lv_timer_pause(this->scrub_hold_timer_);
      if (this->scrub_touch_) {
        this->scrub_touch_ = false;
        lv_obj_add_flag(this->history_screen_.chart().obj(), LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_timer_reset(this->scrub_revert_timer_);  // keep the readout 2 s, then back to live
        lv_timer_resume(this->scrub_revert_timer_);
      }
      break;
    default:
      break;
  }
}

void DialUi::scrub_hold_cb_(lv_timer_t *t) {
  auto *self = static_cast<DialUi *>(lv_timer_get_user_data(t));
  lv_timer_pause(t);
  if (self->page_.kind != PageKind::HISTORY || self->scrub_indev_ == nullptr)
    return;
  lv_point_t p;
  lv_indev_get_point(self->scrub_indev_, &p);
  int32_t dx = p.x - self->scrub_point_.x, dy = p.y - self->scrub_point_.y;
  if (dx * dx + dy * dy > HOLD_SLOP_PX * HOLD_SLOP_PX)
    return;  // moved before the hold: that was a swipe
  self->scrub_encoder_ = false;
  self->scrub_touch_ = true;
  lv_timer_pause(self->scrub_revert_timer_);
  // The drag must not become a swipe: stop the chart's gestures reaching the screen.
  lv_obj_remove_flag(self->history_screen_.chart().obj(), LV_OBJ_FLAG_GESTURE_BUBBLE);
  int i = self->history_screen_.chart().pressed_index();
  self->scrub_to_(i >= 0 ? i : static_cast<int>(HourlyHistory::POINTS) - 1);
}

void DialUi::scrub_revert_cb_(lv_timer_t *t) {
  auto *self = static_cast<DialUi *>(lv_timer_get_user_data(t));
  lv_timer_pause(t);
  if (!self->scrub_touch_)
    self->end_scrub_();
}

void DialUi::scrub_to_(int index) {
  this->history_screen_.scrub(this->rooms_[this->page_.room], index, this->online_last_);
}

void DialUi::end_scrub_() {
  bool active = this->scrub_encoder_ || this->scrub_touch_ || this->history_screen_.scrub_index() >= 0;
  this->scrub_encoder_ = false;
  this->scrub_touch_ = false;
  if (this->scrub_hold_timer_ != nullptr)
    lv_timer_pause(this->scrub_hold_timer_);
  if (this->scrub_revert_timer_ != nullptr)
    lv_timer_pause(this->scrub_revert_timer_);
  lv_obj_t *chart = this->history_screen_.chart().obj();
  if (chart != nullptr)
    lv_obj_add_flag(chart, LV_OBJ_FLAG_GESTURE_BUBBLE);
  if (active && this->page_.kind == PageKind::HISTORY)
    this->scrub_to_(-1);
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
