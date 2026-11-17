#include "dial_ui.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome::dial_ui {

static const char *const TAG = "dial_ui";
static constexpr uint32_t TICK_MS = 6;              // encoder tick length
static constexpr uint32_t POWER_OFF_GRACE_MS = 1000;  // still running after this: on USB
struct SettingsPrefs {
  uint8_t luminosity;
  uint8_t volume;
  uint16_t pitch;
};
static constexpr uint32_t REFRESH_MS = 1000;      // design: update readings at 1 Hz
static constexpr uint32_t SETTINGS_HOLD_MS = 600;  // spec: 600 ms touch hold opens Settings
static constexpr int32_t HOLD_SLOP_PX = 12;        // a hold that moves further is a drag, not a hold
static constexpr uint32_t SAMPLE_MS = 60000;       // history: one sample a minute (contract.md §5)
static constexpr uint32_t SCRUB_HOLD_MS = 250;     // design: hold 250 ms on the chart starts a scrub
static constexpr uint32_t SCRUB_KEEP_MS = 2000;    // design: the readout stays 2 s after release
static constexpr uint32_t FORECAST_EVERY_MS = 30 * 60 * 1000;  // contract.md §6: every 30 min
static constexpr uint32_t FORECAST_TIMEOUT_MS = 30000;         // a request with no answer is retried
// Arrival nudges (spec.md): content arrives moving the way the finger went.
static constexpr int32_t NUDGE_VALUE_PX = 12;      // Sensor hero value (vertical)
static constexpr int32_t NUDGE_ROOM_PX = 8;        // Room readings (vertical)
static constexpr int32_t NUDGE_SETTINGS_PX = 16;   // Settings content (vertical)
static constexpr int32_t NUDGE_SIDE_PX = 12;       // sideways: room to room, metric to metric

// Carousel pages (level 0): Settings opens from them (back, swipe down, touch hold).
static bool is_level0_(const Page &p) {
  return p.kind == PageKind::ROOM || p.kind == PageKind::WEATHER || p.kind == PageKind::POWER;
}

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

void DialUi::add_room(const char *id, const char *name, uint8_t icon) {
  Room r;
  r.id = id;
  r.name = name;
  r.icon = icon;
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

void DialUi::set_statistic_id(const char *entity_id) {
  if (this->rooms_.empty() || this->rooms_.back().readings.empty())
    return;
  this->rooms_.back().readings.back().statistic_id = entity_id;
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

void DialUi::set_weather(sensor::Sensor *temperature, text_sensor::TextSensor *condition) {
  this->has_weather_ = true;
  if (temperature != nullptr)
    temperature->add_on_state_callback([this](float x) {
      this->outdoor_temp_ = x;
      this->weather_dirty_ = true;
    });
  if (condition != nullptr)
    condition->add_on_state_callback([this](const std::string &s) {
      this->condition_ = s;
      this->weather_dirty_ = true;
    });
}

void DialUi::set_rain(text_sensor::TextSensor *forecast, text_sensor::TextSensor *forecast_time) {
  // The two attributes arrive separately; parse the slots when they come, the time when it comes.
  forecast->add_on_state_callback([this](const std::string &s) {
    this->rain_attr_ = s;
    int64_t ref = this->rain_.ref_epoch;
    if (!parse_rain_slots(s.c_str(), &this->rain_)) {
      if (s != "unknown" && s != "unavailable" && !s.empty())
        ESP_LOGW(TAG, "Could not read the rain forecast: '%s'", s.c_str());
      this->rain_.count = 0;
    }
    this->rain_.ref_epoch = ref;
    this->weather_dirty_ = true;
  });
  forecast_time->add_on_state_callback([this](const std::string &s) {
    int64_t t;
    this->rain_.ref_epoch = parse_iso8601(s.c_str(), &t) ? t : -1;
    this->weather_dirty_ = true;
  });
}

void DialUi::set_power(sensor::Sensor *power, float limit_w) {
  this->has_power_ = true;
  this->power_limit_w_ = limit_w;
  power->add_on_state_callback([this](float x) {
    this->power_w_ = x;
    this->power_dirty_ = true;
  });
}

void DialUi::set_energy(sensor::Sensor *energy, const char *statistic_id) {
  this->has_energy_ = true;
  this->energy_statistic_id_ = statistic_id;
  energy->add_on_state_callback([this](float x) {
    if (std::isnan(this->energy_.live()))
      ESP_LOGI(TAG, "Energy counter first value: %.3f", x);
    this->energy_.set_live(x);
    this->energy_dirty_ = true;
  });
}

void DialUi::load_forecast(JsonVariantConst rows) {
  this->forecast_pending_ = false;
  if (!rows.is<JsonArrayConst>()) {
    std::string raw;
    serializeJson(rows, raw);
    ESP_LOGW(TAG, "Forecast response is not a list; check the response_template (contract.md §6). Got: %.200s",
             raw.c_str());
    return;
  }
  this->forecast_.clear();
  for (JsonVariantConst row : rows.as<JsonArrayConst>()) {
    if (!row.is<JsonArrayConst>() || row.size() < 3)
      continue;
    ForecastHour h{};
    h.epoch = row[0].as<int64_t>();
    const char *cond = row[1].as<const char *>();
    snprintf(h.condition, sizeof(h.condition), "%s", cond != nullptr ? cond : "");
    h.temp = row[2].isNull() ? NAN : row[2].as<float>();
    this->forecast_.push_back(h);
    if (this->forecast_.size() >= 30)
      break;
  }
  this->forecast_loaded_ = true;
  this->forecast_loaded_ms_ = millis();
  this->forecast_dirty_ = true;
  ESP_LOGI(TAG, "Forecast received: %u hours", static_cast<unsigned>(this->forecast_.size()));
  if (this->forecast_.empty())
    ESP_LOGW(TAG, "The forecast list is empty: does the weather entity support hourly forecasts?");
}

void DialUi::forecast_request_failed(const std::string &error) {
  this->forecast_pending_ = false;
  ESP_LOGW(TAG, "Forecast request failed: %s", error.c_str());
}

// History backfill (contract.md §5.2): `series` is [["sensor.x", [[hour_epoch, mean], ...]], ...].
void DialUi::load_history(JsonVariantConst series) {
  this->history_pending_ = false;
  if (!series.is<JsonArrayConst>()) {
    std::string raw;
    serializeJson(series, raw);
    ESP_LOGW(TAG, "History response is not a list; check the response_template (contract.md §5.2). Got: %.200s",
             raw.c_str());
    return;
  }
  int64_t now = this->now_epoch_();
  if (now < 0)
    return;
  unsigned loaded = 0, hours = 0;
  for (JsonVariantConst entry : series.as<JsonArrayConst>()) {
    const char *id = entry[0].as<const char *>();
    JsonArrayConst rows = entry[1].as<JsonArrayConst>();
    if (id == nullptr || rows.isNull())
      continue;
    for (auto &room : this->rooms_) {
      for (auto &r : room.readings) {
        if (!r.has_history || r.statistic_id != id)
          continue;
        r.history.advance(now);  // the window must end at the current hour
        for (JsonVariantConst row : rows) {
          if (!row.is<JsonArrayConst>() || row.size() < 2 || row[1].isNull())
            continue;
          r.history.set_closed(row[0].as<int64_t>(), row[1].as<float>());
          hours++;
        }
        loaded++;
      }
    }
  }
  ESP_LOGI(TAG, "History received from Home Assistant: %u series, %u hours", loaded, hours);
  this->redraw_history_views_();
}

void DialUi::history_request_failed(const std::string &error) {
  this->history_pending_ = false;
  ESP_LOGW(TAG, "History request failed: %s", error.c_str());
}

// Energy backfill (contract.md §5.3): `rows` is [[hour_epoch, change_kwh, end_counter_or_null], ...].
void DialUi::load_energy(JsonVariantConst rows) {
  this->energy_pending_ = false;
  if (!rows.is<JsonArrayConst>()) {
    std::string raw;
    serializeJson(rows, raw);
    ESP_LOGW(TAG, "Energy response is not a list; check the response_template (contract.md §5.3). Got: %.200s",
             raw.c_str());
    return;
  }
  int64_t now = this->now_epoch_();
  if (now < 0)
    return;
  this->energy_.advance(now);  // the window must end at the current hour
  unsigned n = 0;
  for (JsonVariantConst row : rows.as<JsonArrayConst>()) {
    if (!row.is<JsonArrayConst>() || row.size() < 2 || row[1].isNull())
      continue;
    double state = row.size() > 2 && !row[2].isNull() ? row[2].as<double>() : static_cast<double>(NAN);
    this->energy_.set_closed(row[0].as<int64_t>(), row[1].as<double>(), state);
    n++;
  }
  ESP_LOGI(TAG, "Energy received from Home Assistant: %u hours; 24 h total %.2f kWh, counter %.3f, hour start %.3f",
           n, this->energy_.total(), this->energy_.live(), this->energy_.base());
  if (n == 0)
    ESP_LOGW(TAG, "No hourly statistics for '%s': it needs a state_class (total_increasing) and kWh as unit. In HA "
                  "Developer tools > Statistics, check that the entity is listed without an error",
             this->energy_statistic_id_.c_str());
  this->energy_dirty_ = true;
}

void DialUi::energy_request_failed(const std::string &error) {
  this->energy_pending_ = false;
  ESP_LOGW(TAG, "Energy request failed: %s", error.c_str());
}

// Once per connection, then once an hour after HA has compiled the previous hour's statistics
// (it does so a few minutes past the hour; a request before that is repeated at minute 6).
void DialUi::maybe_request_energy_() {
  if (!this->has_energy_request_ || !this->online_last_ || this->energy_statistic_id_.empty())
    return;
  int64_t now = this->now_epoch_();
  if (now < 0)
    return;
  uint32_t ms = millis();
  if (this->energy_pending_) {
    if (ms - this->energy_requested_ms_ < 30000)
      return;
    this->energy_pending_ = false;  // no answer: HA did not run the action
    ESP_LOGW(TAG, "No answer to the energy request after 30 s. Check \"Allow the device to perform Home "
                  "Assistant actions\" and the HA log for a rejected recorder.get_statistics call.");
  }
  int64_t hour = now / 3600;
  bool late = now % 3600 >= 6 * 60;
  bool first = this->energy_req_hour_ < 0;
  bool due = hour != this->energy_req_hour_ ? (first || late) : (late && !this->energy_req_late_);
  if (!due)
    return;
  this->energy_req_hour_ = hour;
  this->energy_req_late_ = late;
  this->energy_pending_ = true;
  this->energy_requested_ms_ = ms;
  ESP_LOGI(TAG, "Requesting hourly energy (recorder.get_statistics) for %s", this->energy_statistic_id_.c_str());
  this->energy_request_trigger_.trigger(this->energy_statistic_id_);
}

void DialUi::refresh_energy_() {
  int64_t now = this->now_epoch_();
  if (now >= 0 && this->energy_.advance(now))
    this->energy_dirty_ = true;  // the hour rolled over: the window moved
  if (this->energy_dirty_ && this->page_.kind == PageKind::ENERGY) {
    this->energy_dirty_ = false;
    this->energy_screen_.update(this->energy_, this->online_last_);
  }
}

void DialUi::redraw_history_views_() {
  if (this->page_.kind == PageKind::HISTORY)
    this->history_screen_.update(this->rooms_[this->page_.room], this->online_last_);
  if (this->page_.kind == PageKind::SENSOR)
    this->visible_sensor_()->update_trend(this->rooms_[this->page_.room]);
}

// Once per connection, as soon as the clock is set: closed hours from HA's statistics.
void DialUi::maybe_request_history_() {
  if (!this->has_history_request_ || !this->online_last_ || this->history_requested_)
    return;
  if (this->now_epoch_() < 0)
    return;  // the window is placed by the clock
  std::string ids;
  for (const auto &room : this->rooms_)
    for (const auto &r : room.readings)
      if (r.has_history && !r.statistic_id.empty()) {
        if (!ids.empty())
          ids += ',';
        ids += r.statistic_id;
      }
  this->history_requested_ = true;
  if (ids.empty())
    return;
  this->history_pending_ = true;
  this->history_requested_ms_ = millis();
  ESP_LOGI(TAG, "Requesting 24 h of history (recorder.get_statistics) for %s", ids.c_str());
  this->history_request_trigger_.trigger(ids);
}

int64_t DialUi::now_epoch_() const {
  if (this->time_ == nullptr)
    return -1;
  int64_t t = static_cast<int64_t>(this->time_->timestamp_now());
  return t >= HourlyHistory::MIN_VALID_EPOCH ? t : -1;
}

// Forecast requests (contract.md §6): on connect, every 30 min, and when the Forecast page
// opens with data older than 30 min. One request at a time.
void DialUi::maybe_request_forecast_(bool force) {
  if (!this->has_weather_ || !this->online_last_)
    return;
  uint32_t now = millis();
  if (this->forecast_pending_) {
    if (now - this->forecast_requested_ms_ < FORECAST_TIMEOUT_MS)
      return;
    // No on_success / on_error at all: Home Assistant did not run the action.
    ESP_LOGW(TAG, "No answer to the forecast request after %u s. Check that \"Allow the device to perform "
                  "Home Assistant actions\" is on for this device in HA (ESPHome integration options), and the "
                  "HA log / repairs for a rejected weather.get_forecasts call.",
             static_cast<unsigned>(FORECAST_TIMEOUT_MS / 1000));
    this->forecast_pending_ = false;
  }
  bool stale = !this->forecast_loaded_ || now - this->forecast_loaded_ms_ >= FORECAST_EVERY_MS;
  if (!force && !stale)
    return;
  this->forecast_pending_ = true;
  this->forecast_requested_ms_ = now;
  ESP_LOGI(TAG, "Requesting the hourly forecast (weather.get_forecasts)");
  this->forecast_request_trigger_.trigger();
}

void DialUi::refresh_weather_() {
  if (!this->has_weather_)
    return;
  int64_t now = this->now_epoch_();
  RainRun runs[RainArc::MAX_RUNS];
  size_t n = now >= 0 ? rain_runs(this->rain_, now, runs, RainArc::MAX_RUNS) : 0;
  // Known = slots parsed, a reference time, and not older than the forecast's own hour.
  bool rain_known = now >= 0 && this->rain_.valid() && now - this->rain_.ref_epoch < RainForecast::HORIZON_MIN * 60;
  this->weather_screen_.update(this->outdoor_temp_, this->condition_, runs, n, rain_known, this->online_last_);
  if (this->forecast_dirty_) {  // new rows: redraw the strip (it keeps its focus)
    this->forecast_dirty_ = false;
    size_t first = now >= 0 ? forecast_first(this->forecast_.data(), this->forecast_.size(), now) : 0;
    size_t n_rows = this->forecast_.size() - first;
    if (n_rows > ScreenForecast::MAX_HOURS)
      n_rows = ScreenForecast::MAX_HOURS;
    this->forecast_screen_.set_data(this->forecast_.data() + first, n_rows, now);
  }
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
    this->room_screens_[r].set_dots(carousel_size(this->shape_()), r);
    this->attach_events_(this->room_screens_[r].screen());
  }
  for (auto &scr : this->sensor_screens_) {
    scr.create();
    this->attach_events_(scr.screen());
  }
  // Saved settings (ESPHome preferences, not HA).
  this->luminosity_ = adjust_spec(SettingsRow::LUMINOSITY).def;
  this->volume_ = adjust_spec(SettingsRow::BUZZER).def;
  this->pitch_ = adjust_spec(SettingsRow::BUZZER_PITCH).def;
  // v2: pitch added (a new key, so an old 2-field record is not misread; it resets to defaults once).
  this->pref_ = global_preferences->make_preference<SettingsPrefs>(fnv1_hash("dial_ui_settings_v2"));
  SettingsPrefs saved;
  if (this->pref_.load(&saved)) {
    this->luminosity_ = clamp_setting(adjust_spec(SettingsRow::LUMINOSITY), saved.luminosity);
    this->volume_ = clamp_setting(adjust_spec(SettingsRow::BUZZER), saved.volume);
    this->pitch_ = clamp_setting(adjust_spec(SettingsRow::BUZZER_PITCH), saved.pitch);
  }
  this->apply_luminosity_(this->luminosity_);
  this->apply_pitch_(this->pitch_);

  this->settings_screen_.create();
  this->settings_screen_.set_values(this->luminosity_, this->volume_, this->pitch_);
  this->attach_events_(this->settings_screen_.screen());
  this->adjust_screen_.create();
  this->attach_events_(this->adjust_screen_.screen());
  lv_obj_add_event_cb(this->adjust_screen_.arc(), &DialUi::adjust_event_cb_, LV_EVENT_VALUE_CHANGED, this);
  this->history_screen_.create();
  this->attach_events_(this->history_screen_.screen());
  lv_obj_t *chart = this->history_screen_.chart().obj();
  for (lv_event_code_t c : {LV_EVENT_PRESSED, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
    lv_obj_add_event_cb(chart, &DialUi::chart_event_cb_, c, this);
  this->scrub_hold_timer_ = lv_timer_create(&DialUi::scrub_hold_cb_, SCRUB_HOLD_MS, this);
  lv_timer_pause(this->scrub_hold_timer_);
  this->scrub_revert_timer_ = lv_timer_create(&DialUi::scrub_revert_cb_, SCRUB_KEEP_MS, this);
  lv_timer_pause(this->scrub_revert_timer_);

  if (this->has_weather_) {
    this->weather_screen_.create();
    this->attach_events_(this->weather_screen_.screen());
    this->forecast_screen_.create();
    this->attach_events_(this->forecast_screen_.screen());
  }

  if (this->has_power_) {
    this->power_screen_.create(this->power_limit_w_, this->has_energy_);
    this->attach_events_(this->power_screen_.screen());
    if (this->has_energy_) {
      this->energy_screen_.create();
      this->attach_events_(this->energy_screen_.screen());
      lv_obj_t *bars = this->energy_screen_.chart().obj();
      for (lv_event_code_t c : {LV_EVENT_PRESSED, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
        lv_obj_add_event_cb(bars, &DialUi::chart_event_cb_, c, this);
    }
  }

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
  this->update_sleep_();
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
    this->weather_dirty_ = true;
    this->power_dirty_ = true;
    this->energy_dirty_ = true;
    if (online) {
      this->maybe_request_forecast_(true);  // on connect
    } else {
      this->history_requested_ = false;  // ask again on the next connection (fills the gap)
      this->energy_req_hour_ = -1;
    }
  }
  this->refresh_();
  if (this->has_power_ && this->power_dirty_) {  // power at 1 Hz (design)
    this->power_dirty_ = false;
    this->power_screen_.update(this->power_w_, this->online_last_, this->page_.kind == PageKind::POWER);
  }
  this->maybe_request_forecast_(false);  // every 30 min
  this->maybe_request_history_();
  if (this->has_energy_) {
    this->refresh_energy_();
    this->maybe_request_energy_();
  }
  if (this->history_pending_ && now - this->history_requested_ms_ >= 30000) {
    this->history_pending_ = false;  // no answer: HA did not run the action
    ESP_LOGW(TAG, "No answer to the history request after 30 s. Check \"Allow the device to perform Home "
                  "Assistant actions\" and the HA log for a rejected recorder.get_statistics call.");
  }
  // The rain arc moves with the clock (past slots fall off): redraw once a minute too.
  uint32_t minute = now / 60000;
  if (this->weather_dirty_ || minute != this->last_rain_minute_) {
    this->weather_dirty_ = false;
    this->last_rain_minute_ = minute;
    this->refresh_weather_();
  } else if (this->forecast_dirty_) {
    this->refresh_weather_();
  }
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
  this->redraw_history_views_();
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

NavShape DialUi::shape_() const {
  NavShape s{this->rooms_.size(), this->readings_per_room_.data(), this->history_mask_.data()};
  s.has_weather = this->has_weather_;
  s.has_power = this->has_power_;
  s.has_energy = this->has_energy_;
  return s;
}

void DialUi::nav_(NavAction action) {
  NavResult r = nav_apply(this->page_, action, this->shape_(), &this->before_settings_);
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
      NavShape shape = this->shape_();
      // Dots glide when moving along the carousel; otherwise they are placed directly.
      bool along = is_level0_(prev) && !(prev == page);
      if (along)
        scr.animate_dots(carousel_size(shape), carousel_index(prev, shape), page.room);
      else
        scr.set_dots(carousel_size(shape), page.room);
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
    case PageKind::WEATHER: {
      NavShape shape = this->shape_();
      size_t idx = carousel_index(page, shape);
      if (is_level0_(prev) && !(prev == page))
        this->weather_screen_.animate_dots(carousel_size(shape), carousel_index(prev, shape), idx);
      else
        this->weather_screen_.set_dots(carousel_size(shape), idx);
      this->refresh_weather_();
      this->weather_screen_.nudge(arrival_dx(slide, NUDGE_SIDE_PX), arrival_dy(slide, NUDGE_ROOM_PX));
      this->load_(this->weather_screen_.screen(), slide);
      break;
    }
    case PageKind::POWER: {
      NavShape shape = this->shape_();
      size_t idx = carousel_index(page, shape);
      if (is_level0_(prev) && !(prev == page))
        this->power_screen_.animate_dots(carousel_size(shape), carousel_index(prev, shape), idx);
      else
        this->power_screen_.set_dots(carousel_size(shape), idx);
      // Ring sweep on arrival, like a Sensor page (only the ring redraws).
      this->power_screen_.reset_ring();
      this->power_screen_.update(this->power_w_, this->online_last_, true);
      this->power_screen_.nudge(arrival_dx(slide, NUDGE_SIDE_PX), arrival_dy(slide, NUDGE_VALUE_PX));
      this->load_(this->power_screen_.screen(), slide);
      break;
    }
    case PageKind::ENERGY:
      this->energy_screen_.update(this->energy_, this->online_last_);
      this->energy_dirty_ = false;
      this->energy_screen_.nudge(arrival_dy(slide, NUDGE_VALUE_PX));
      this->load_(this->energy_screen_.screen(), slide);
      break;
    case PageKind::FORECAST: {
      this->maybe_request_forecast_(false);  // refresh if older than 30 min
      int64_t now = this->now_epoch_();
      size_t first = now >= 0 ? forecast_first(this->forecast_.data(), this->forecast_.size(), now) : 0;
      size_t n_rows = this->forecast_.size() - first;
      if (n_rows > ScreenForecast::MAX_HOURS)
        n_rows = ScreenForecast::MAX_HOURS;
      this->forecast_screen_.set_data(this->forecast_.data() + first, n_rows, now);
      this->forecast_screen_.focus(0);  // opens at "Now" (design ScreenForecast)
      this->forecast_screen_.nudge(arrival_dy(slide, NUDGE_VALUE_PX));
      this->load_(this->forecast_screen_.screen(), slide);
      break;
    }
    case PageKind::ADJUST:
      this->adjust_screen_.bind(adjust_spec(static_cast<SettingsRow>(page.index)), this->adjust_value_);
      this->adjust_screen_.nudge(arrival_dy(slide, NUDGE_VALUE_PX));
      this->load_(this->adjust_screen_.screen(), slide);
      break;
    case PageKind::SETTINGS:
      // Leaving Adjust without saving (cancel) restores the saved luminosity and pitch.
      this->apply_luminosity_(this->luminosity_);
      this->apply_pitch_(this->pitch_);
      if (prev.kind != PageKind::ADJUST)
        this->settings_screen_.set_focus(0);  // Settings opens on its first row
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
                                          LV_EVENT_PRESS_LOST, LV_EVENT_PRESSING};
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

  // Screen off: any input only wakes the screen and is not acted on (spec.md "Behaviour").
  if (this->screen_off_state_) {
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_KEY || code == LV_EVENT_GESTURE) {
      this->wake_();
      if (from_touch)
        lv_indev_wait_release(indev);
      else if (code == LV_EVENT_PRESSED)
        this->swallow_encoder_click_ = 1;
    }
    return;
  }
  // The encoder press that woke the screen: drop it until its release, then drop the
  // SHORT_CLICKED / CLICKED that LVGL sends after that release (lv_indev.c order).
  if (from_encoder && this->swallow_encoder_click_ == 1) {
    if (code == LV_EVENT_RELEASED)
      this->swallow_encoder_click_ = 2;
    if (code != LV_EVENT_KEY)
      return;
  } else if (from_encoder && this->swallow_encoder_click_ == 2) {
    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_CLICKED)
      return;
    this->swallow_encoder_click_ = 0;
  }
  // Encoder tick (user: encoder only, not touch). On Adjust the KEY tick plays after the change.
  bool adjusting_key = this->page_.kind == PageKind::ADJUST && code == LV_EVENT_KEY;
  if (from_encoder && (code == LV_EVENT_KEY || code == LV_EVENT_PRESSED) && !adjusting_key)
    this->tick_();

  if ((this->page_.kind == PageKind::SETTINGS || this->page_.kind == PageKind::ADJUST) &&
      this->settings_event_(e, code, from_encoder))
    return;
  if (is_scrub_page_(this->page_.kind) && this->scrub_event_(e, code, from_encoder))
    return;
  if (this->page_.kind == PageKind::FORECAST && code == LV_EVENT_KEY) {
    // Turn moves the focus one hour; it never changes page here.
    uint32_t key = lv_event_get_key(e);
    this->forecast_screen_.step(key == LV_KEY_RIGHT ? 1 : (key == LV_KEY_LEFT ? -1 : 0));
    return;
  }

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
      } else if (from_touch && this->page_.kind == PageKind::SENSOR && target == this->visible_sensor_()->value_obj()) {
        this->nav_(NavAction::OPEN);  // tap on the value: History (only if the metric keeps history)
      } else if (from_touch && this->page_.kind == PageKind::POWER && target == this->power_screen_.value_obj()) {
        this->nav_(NavAction::OPEN);  // tap on the power value: Energy (only if configured)
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
      if (from_touch && is_level0_(this->page_)) {
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

// ---- Scrub on History and Energy (design ScreenHistory; spec.md "Level 2 interaction pages") ----
// Both pages show the same 25-point window (HourlyHistory::POINTS == EnergyHistory::POINTS).
static_assert(HourlyHistory::POINTS == EnergyHistory::POINTS, "History and Energy share the scrub range");
static constexpr int SCRUB_POINTS = static_cast<int>(HourlyHistory::POINTS);

lv_obj_t *DialUi::scrub_chart_obj_() {
  return this->page_.kind == PageKind::ENERGY ? this->energy_screen_.chart().obj() : this->history_screen_.chart().obj();
}

int DialUi::scrub_index_() const {
  return this->page_.kind == PageKind::ENERGY ? this->energy_screen_.scrub_index() : this->history_screen_.scrub_index();
}

// The point under the finger, as LVGL recorded it on LV_EVENT_PRESSED. It does not follow a drag
// (lv_chart.c only updates it on press), so touch selects one point; there is no drag scrub
// (user, 2026-11-17).
int DialUi::scrub_pressed_index_() {
  return this->page_.kind == PageKind::ENERGY ? this->energy_screen_.chart().pressed_index()
                                              : this->history_screen_.chart().pressed_index();
}

int DialUi::scrub_latest_index_() const {
  if (this->page_.kind == PageKind::ENERGY)
    return this->energy_.latest_index();
  return this->rooms_[this->page_.room].readings[this->page_.index].history.latest_index();
}

// Returns true when the event was consumed by the History or Energy page.
bool DialUi::scrub_event_(lv_event_t *e, lv_event_code_t code, bool from_encoder) {
  if (code == LV_EVENT_KEY) {  // turn never changes page here; while scrubbing it moves the cursor
    if (this->scrub_encoder_) {
      int i = this->scrub_index_();
      uint32_t key = lv_event_get_key(e);
      if (key == LV_KEY_RIGHT && i + 1 < SCRUB_POINTS)
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
      int latest = this->scrub_latest_index_();
      this->scrub_encoder_ = true;  // press starts the scrub on the latest sample
      this->scrub_to_(latest >= 0 ? latest : SCRUB_POINTS - 1);
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
  if (!is_scrub_page_(this->page_.kind))
    return;
  lv_indev_t *indev = lv_indev_active();
  if (indev == nullptr || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER)
    return;
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      // A hold of 250 ms on a point shows it; a quick swipe before that stays a swipe.
      this->scrub_indev_ = indev;
      lv_indev_get_point(indev, &this->scrub_point_);
      lv_timer_reset(this->scrub_hold_timer_);
      lv_timer_resume(this->scrub_hold_timer_);
      break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
      lv_timer_pause(this->scrub_hold_timer_);
      if (this->scrub_touch_) {
        this->scrub_touch_ = false;
        lv_obj_add_flag(this->scrub_chart_obj_(), LV_OBJ_FLAG_GESTURE_BUBBLE);
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
  if (!is_scrub_page_(self->page_.kind) || self->scrub_indev_ == nullptr)
    return;
  lv_point_t p;
  lv_indev_get_point(self->scrub_indev_, &p);
  int32_t dx = p.x - self->scrub_point_.x, dy = p.y - self->scrub_point_.y;
  if (dx * dx + dy * dy > HOLD_SLOP_PX * HOLD_SLOP_PX)
    return;  // moved before the hold: that was a swipe
  self->scrub_encoder_ = false;
  self->scrub_touch_ = true;
  lv_timer_pause(self->scrub_revert_timer_);
  // A finger moving after the hold must not become a swipe: stop the chart's gestures reaching the screen.
  lv_obj_remove_flag(self->scrub_chart_obj_(), LV_OBJ_FLAG_GESTURE_BUBBLE);
  int i = self->scrub_pressed_index_();
  self->scrub_to_(i >= 0 ? i : SCRUB_POINTS - 1);
}

void DialUi::scrub_revert_cb_(lv_timer_t *t) {
  auto *self = static_cast<DialUi *>(lv_timer_get_user_data(t));
  lv_timer_pause(t);
  if (!self->scrub_touch_)
    self->end_scrub_();
}

void DialUi::scrub_to_(int index) {
  if (this->page_.kind == PageKind::ENERGY)
    this->energy_screen_.scrub(this->energy_, index, this->online_last_);
  else if (this->page_.kind == PageKind::HISTORY)
    this->history_screen_.scrub(this->rooms_[this->page_.room], index, this->online_last_);
}

void DialUi::end_scrub_() {
  bool active = this->scrub_encoder_ || this->scrub_touch_ || this->scrub_index_() >= 0;
  this->scrub_encoder_ = false;
  this->scrub_touch_ = false;
  if (this->scrub_hold_timer_ != nullptr)
    lv_timer_pause(this->scrub_hold_timer_);
  if (this->scrub_revert_timer_ != nullptr)
    lv_timer_pause(this->scrub_revert_timer_);
  for (lv_obj_t *chart : {this->history_screen_.chart().obj(), this->energy_screen_.chart().obj()})
    if (chart != nullptr)
      lv_obj_add_flag(chart, LV_OBJ_FLAG_GESTURE_BUBBLE);
  if (active && is_scrub_page_(this->page_.kind))
    this->scrub_to_(-1);
}

// ---- Settings (spec.md "Settings list"; design ListMenu, ScreenAdjust) ----

bool DialUi::settings_event_(lv_event_t *e, lv_event_code_t code, bool from_encoder) {
  lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
  if (this->page_.kind == PageKind::SETTINGS) {
    // Touch on the scroll arc: press or drag along it to pick the row. The swipe it makes is not
    // a "close Settings" gesture.
    lv_indev_t *indev = lv_indev_active();
    if (indev != nullptr && lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
      lv_point_t p;
      lv_indev_get_point(indev, &p);
      if (code == LV_EVENT_PRESSED) {
        this->settings_drag_ = target == this->settings_screen_.screen() &&
                               this->settings_screen_.row_at_point(p.x, p.y, true) >= 0;
      }
      if (this->settings_drag_) {
        if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
          int row = this->settings_screen_.row_at_point(p.x, p.y, false);
          size_t f = this->settings_screen_.focus();
          if (row >= 0 && static_cast<size_t>(row) != f)
            this->settings_screen_.set_focus(static_cast<size_t>(row), static_cast<size_t>(row) > f ? 1 : -1);
        }
        if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
          this->settings_drag_ = false;
        return true;  // gestures and the release tap are swallowed while dragging
      }
    }
    if (code == LV_EVENT_KEY) {  // turn moves the row focus
      uint32_t key = lv_event_get_key(e);
      size_t f = this->settings_screen_.focus();
      if (key == LV_KEY_RIGHT && f + 1 < SETTINGS_ROWS)
        this->settings_screen_.set_focus(f + 1, 1);
      else if (key == LV_KEY_LEFT && f > 0)
        this->settings_screen_.set_focus(f - 1, -1);
      return true;
    }
    if (code == LV_EVENT_SHORT_CLICKED) {
      if (from_encoder) {
        this->activate_row_(static_cast<SettingsRow>(this->settings_screen_.focus()));
        return true;
      }
      // Touch: a tap on the focused row opens it; a tap on a neighbour scrolls it into focus
      // (swipe up closes Settings, so a swipe cannot scroll the list).
      int row = this->settings_screen_.row_for(target);
      if (row < 0)
        return true;
      if (static_cast<size_t>(row) == this->settings_screen_.focus())
        this->activate_row_(static_cast<SettingsRow>(row));
      else
        this->settings_screen_.set_focus(static_cast<size_t>(row),
                                         static_cast<size_t>(row) > this->settings_screen_.focus() ? 1 : -1);
      return true;
    }
    return false;  // long press, swipes: generic (back closes Settings)
  }
  // ADJUST
  if (code == LV_EVENT_KEY) {  // one detent = one step, accelerated on fast turns
    uint32_t key = lv_event_get_key(e);
    int dir = key == LV_KEY_RIGHT ? 1 : (key == LV_KEY_LEFT ? -1 : 0);
    if (dir != 0)
      this->adjust_by_(dir * this->accel_.steps(millis(), dir));
    this->tick_();  // after the change: the buzzer page previews the new volume
    return true;
  }
  if (code == LV_EVENT_SHORT_CLICKED && (from_encoder || target == this->adjust_screen_.ok_button())) {
    this->save_settings_();  // press or OK saves
    return true;
  }
  return false;  // long press / swipe down: back = cancel
}

void DialUi::adjust_event_cb_(lv_event_t *e) {
  auto *self = static_cast<DialUi *>(lv_event_get_user_data(e));
  if (self->page_.kind != PageKind::ADJUST)
    return;
  int v = self->adjust_screen_.value_from_arc();  // touch drag on the ring
  self->adjust_value_ = v;
  self->adjust_screen_.set_value(v);
  if (static_cast<SettingsRow>(self->page_.index) == SettingsRow::LUMINOSITY)
    self->apply_luminosity_(self->adjust_screen_.value());
  if (static_cast<SettingsRow>(self->page_.index) == SettingsRow::BUZZER_PITCH)
    self->apply_pitch_(self->adjust_screen_.value());
}

void DialUi::adjust_by_(int delta_steps) {
  if (delta_steps == 0)
    return;
  SettingsRow row = static_cast<SettingsRow>(this->page_.index);
  const AdjustSpec &spec = adjust_spec(row);
  this->adjust_value_ = setting_step(spec, this->adjust_value_, delta_steps);
  this->adjust_screen_.set_value(this->adjust_value_);
  // Live preview, undone on cancel (design: never change a value live without a way to cancel).
  if (row == SettingsRow::LUMINOSITY)
    this->apply_luminosity_(this->adjust_value_);
  if (row == SettingsRow::BUZZER_PITCH)
    this->apply_pitch_(this->adjust_value_);  // the tick after the change previews it
}

void DialUi::open_adjust_(SettingsRow row) {
  this->adjust_value_ = row == SettingsRow::LUMINOSITY ? this->luminosity_
                        : row == SettingsRow::BUZZER    ? this->volume_
                                                        : this->pitch_;
  this->show_(Page{PageKind::ADJUST, 0, static_cast<size_t>(row)}, Slide::UP);
}

void DialUi::save_settings_() {
  SettingsRow row = static_cast<SettingsRow>(this->page_.index);
  if (row == SettingsRow::LUMINOSITY)
    this->luminosity_ = this->adjust_value_;
  else if (row == SettingsRow::BUZZER)
    this->volume_ = this->adjust_value_;
  else
    this->pitch_ = this->adjust_value_;
  SettingsPrefs p{static_cast<uint8_t>(this->luminosity_), static_cast<uint8_t>(this->volume_),
                  static_cast<uint16_t>(this->pitch_)};
  this->pref_.save(&p);
  this->settings_screen_.set_values(this->luminosity_, this->volume_, this->pitch_);
  ESP_LOGI(TAG, "Saved: luminosity %d %%, buzzer volume %d %%, pitch %d Hz", this->luminosity_, this->volume_,
           this->pitch_);
  this->show_(Page{PageKind::SETTINGS, 0, 0}, Slide::DOWN);
}

void DialUi::activate_row_(SettingsRow row) {
  switch (row) {
    case SettingsRow::LUMINOSITY:
    case SettingsRow::BUZZER:
    case SettingsRow::BUZZER_PITCH:
      this->open_adjust_(row);
      break;
    case SettingsRow::SCREEN_OFF:
      this->nav_(NavAction::BACK);  // back to the page you left, then dark
      this->screen_off_();
      break;
    case SettingsRow::POWER_OFF:
      this->nav_(NavAction::BACK);
      if (this->power_hold_ == nullptr) {
        ESP_LOGW(TAG, "Power off: no power_hold switch configured");
        break;
      }
      // HOLD low cuts power on battery. On USB the device keeps running: fall back to screen off,
      // and HOLD goes high again on wake, so unplugging later does not cut power by surprise.
      ESP_LOGI(TAG, "Power off (HOLD low)");
      this->hold_released_ = true;
      this->power_hold_->turn_off();
      this->set_timeout("power_off", POWER_OFF_GRACE_MS, [this]() { this->screen_off_(); });
      break;
    case SettingsRow::EXIT:
      this->nav_(NavAction::BACK);
      break;
  }
}

void DialUi::apply_luminosity_(int percent) {
  if (this->backlight_ == nullptr)
    return;
  auto call = this->backlight_->make_call();
  call.set_state(true);
  call.set_brightness(static_cast<float>(percent) / 100.0f);
  call.set_transition_length(0);
  call.perform();
}

// Dim after 30 s without input, off after 60 s (spec.md "Behaviour"). LVGL records the last input
// (any press, touch or button, and any encoder turn: lv_indev.c), so no input hook is needed.
// Data updates and alerts never count as input, so they never wake the screen.
void DialUi::update_sleep_() {
  if (this->parent_ == nullptr || this->parent_->get_disp() == nullptr)
    return;
  switch (this->sleep_.update(lv_display_get_inactive_time(this->parent_->get_disp()))) {
    case SleepAction::DIM:
      ESP_LOGD(TAG, "No input: dim");
      this->apply_luminosity_(dim_level(this->current_luminosity_()));
      break;
    case SleepAction::RESTORE:
      this->apply_luminosity_(this->current_luminosity_());
      break;
    case SleepAction::TURN_OFF:
      ESP_LOGD(TAG, "No input: screen off");
      this->screen_off_();
      break;
    default:
      break;
  }
}

// The brightness the screen should have when awake: the value being previewed on the
// Luminosity Adjust page, otherwise the saved one.
int DialUi::current_luminosity_() const {
  if (this->page_.kind == PageKind::ADJUST && static_cast<SettingsRow>(this->page_.index) == SettingsRow::LUMINOSITY)
    return this->adjust_value_;
  return this->luminosity_;
}

void DialUi::screen_off_() {
  this->screen_off_state_ = true;
  this->sleep_.force_off();
  if (this->backlight_ != nullptr) {
    auto call = this->backlight_->make_call();
    call.set_state(false);
    call.set_transition_length(0);
    call.perform();
  }
}

void DialUi::wake_() {
  this->screen_off_state_ = false;
  this->sleep_.wake();
  this->cancel_timeout("power_off");
  if (this->hold_released_ && this->power_hold_ != nullptr) {
    this->power_hold_->turn_on();
    this->hold_released_ = false;
  }
  this->apply_luminosity_(this->current_luminosity_());
}

void DialUi::apply_pitch_(int hz) {
  if (this->buzzer_ != nullptr)
    this->buzzer_->update_frequency(static_cast<float>(hz));  // LEDC: changes the PWM frequency
}

void DialUi::tick_() {
  if (this->buzzer_ == nullptr)
    return;
  // On the buzzer's Adjust page the tick previews the value being edited.
  int vol = (this->page_.kind == PageKind::ADJUST &&
             static_cast<SettingsRow>(this->page_.index) == SettingsRow::BUZZER)
                ? this->adjust_value_
                : this->volume_;
  if (vol <= 0)
    return;
  this->buzzer_->set_level(buzzer_level(vol));
  this->set_timeout("tick", TICK_MS, [this]() { this->buzzer_->set_level(0.0f); });
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
  if (indev == nullptr || !is_level0_(self->page_))
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
