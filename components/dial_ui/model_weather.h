#pragma once
// Weather model (spec.md "Weather", contract.md §6). Pure C++, host-tested: HA condition ->
// icon and word, Météo-France next-hour rain parsing, the rain arc runs and the rain line.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace esphome::dial_ui {

// The six design glyphs (design/components/WeatherIcon), in weather_icons.c order.
enum class WeatherIcon : uint8_t { CLEAR, PART_CLOUDY, CLOUDY, RAIN, SNOW, STORM };

struct ConditionInfo {
  WeatherIcon icon;
  const char *word;  // condition word shown next to the icon, <= 12 characters
};

// HA weather condition (homeassistant/components/weather/const.py) -> icon and word.
// Icon mapping decided in spec.md; fog, wind and hail keep the nearest glyph and say it in words.
inline bool condition_info(const char *ha, ConditionInfo *out) {
  static const struct {
    const char *ha;
    ConditionInfo info;
  } TABLE[] = {
      {"sunny", {WeatherIcon::CLEAR, "Sunny"}},
      {"clear-night", {WeatherIcon::CLEAR, "Clear"}},
      {"partlycloudy", {WeatherIcon::PART_CLOUDY, "Part cloudy"}},
      {"windy-variant", {WeatherIcon::PART_CLOUDY, "Windy"}},
      {"cloudy", {WeatherIcon::CLOUDY, "Cloudy"}},
      {"fog", {WeatherIcon::CLOUDY, "Fog"}},
      {"windy", {WeatherIcon::CLOUDY, "Windy"}},
      {"exceptional", {WeatherIcon::CLOUDY, "Exceptional"}},
      {"rainy", {WeatherIcon::RAIN, "Rain"}},
      {"pouring", {WeatherIcon::RAIN, "Heavy rain"}},
      {"snowy", {WeatherIcon::SNOW, "Snow"}},
      {"snowy-rainy", {WeatherIcon::SNOW, "Sleet"}},
      {"hail", {WeatherIcon::SNOW, "Hail"}},
      {"lightning", {WeatherIcon::STORM, "Storm"}},
      {"lightning-rainy", {WeatherIcon::STORM, "Storm"}},
  };
  if (ha == nullptr)
    return false;
  for (const auto &row : TABLE) {
    if (std::strcmp(ha, row.ha) == 0) {
      *out = row.info;
      return true;
    }
  }
  return false;
}

// ---- Time ----

// Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm).
inline int64_t days_from_civil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

// "2026-11-15T21:05:00+00:00" / "...Z" / "...+01:00" (as sent by HA) -> epoch seconds.
inline bool parse_iso8601(const char *s, int64_t *epoch) {
  int y, mo, d, h, mi, sec;
  int n = 0;
  if (s == nullptr || std::sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d%n", &y, &mo, &d, &h, &mi, &sec, &n) != 6)
    return false;
  const char *p = s + n;
  if (*p == '.')  // fractional seconds
    while (*++p >= '0' && *p <= '9') {
    }
  int64_t offset = 0;
  if (*p == '+' || *p == '-') {
    int oh, om;
    if (std::sscanf(p + 1, "%2d:%2d", &oh, &om) != 2)
      return false;
    offset = (oh * 3600 + om * 60) * (*p == '-' ? -1 : 1);
  } else if (*p != 'Z' && *p != '\0') {
    return false;
  }
  *epoch = days_from_civil(y, mo, d) * 86400 + h * 3600 + mi * 60 + sec - offset;
  return true;
}

// ---- Météo-France next-hour rain (meteo_france/sensor.py, MeteoFranceRainSensor) ----

// One slot of the `1_hour_forecast` attribute: {"0 min": "Temps sec", "5 min": "Pluie faible", ...}.
// Rain levels: Météo-France's `rain` 1..4 ("Temps sec", "Pluie faible", "Pluie modérée",
// "Pluie forte"; meteofrance-api tests/test_rain.py). HA's attribute only carries the text.
enum RainLevel : uint8_t { RAIN_DRY = 0, RAIN_LIGHT = 1, RAIN_MODERATE = 2, RAIN_HEAVY = 3 };

// Text -> level. Any other text that is not "Temps sec" counts as moderate rain.
inline uint8_t rain_level(const std::string &text) {
  if (text == "Temps sec")
    return RAIN_DRY;
  if (text == "Pluie faible")
    return RAIN_LIGHT;
  if (text == "Pluie forte")
    return RAIN_HEAVY;
  return RAIN_MODERATE;  // "Pluie modérée", or an unknown rain text
}

struct RainSlot {
  int16_t start_min;  // minutes after forecast_time_ref
  uint8_t level;      // RainLevel
  bool rainy() const { return this->level != RAIN_DRY; }
};

struct RainForecast {
  static constexpr size_t MAX_SLOTS = 16;
  static constexpr int HORIZON_MIN = 60;  // the forecast covers one hour from forecast_time_ref
  int64_t ref_epoch{-1};
  RainSlot slots[MAX_SLOTS]{};
  size_t count{0};
  bool valid() const { return this->ref_epoch >= 0 && this->count > 0; }
};

// Reads a quoted Python string ('...' or "...") at *p; advances past it.
inline bool read_py_string_(const char *&p, std::string *out) {
  while (*p == ' ')
    p++;
  char q = *p;
  if (q != '\'' && q != '"')
    return false;
  const char *start = ++p;
  while (*p != '\0' && *p != q)
    p++;
  if (*p != q)
    return false;
  out->assign(start, static_cast<size_t>(p - start));
  p++;
  return true;
}

// Parses the attribute as HA sends it: Python str() of a dict (contract.md §3.3).
// A slot is dry when its text is exactly "Temps sec" (spec.md); the rain texts give the level.
inline bool parse_rain_slots(const char *text, RainForecast *out) {
  out->count = 0;
  if (text == nullptr)
    return false;
  const char *p = text;
  while (*p == ' ')
    p++;
  if (*p++ != '{')
    return false;
  std::string key, value;
  while (true) {
    while (*p == ' ' || *p == ',')
      p++;
    if (*p == '}')
      break;
    if (!read_py_string_(p, &key))
      return false;
    while (*p == ' ')
      p++;
    if (*p++ != ':')
      return false;
    if (!read_py_string_(p, &value))
      return false;
    int minutes;
    if (std::sscanf(key.c_str(), "%d min", &minutes) != 1 || minutes < 0 || minutes >= RainForecast::HORIZON_MIN)
      return false;
    if (out->count < RainForecast::MAX_SLOTS)
      out->slots[out->count++] = RainSlot{static_cast<int16_t>(minutes), rain_level(value)};
  }
  // Slots are in time order in the attribute; keep them sorted anyway.
  for (size_t i = 1; i < out->count; i++)
    for (size_t j = i; j > 0 && out->slots[j - 1].start_min > out->slots[j].start_min; j--) {
      RainSlot t = out->slots[j];
      out->slots[j] = out->slots[j - 1];
      out->slots[j - 1] = t;
    }
  return out->count > 0;
}

// A rainy stretch of one level on the 0..60 min timeline that starts now (arc segments).
struct RainRun {
  float from_min;
  float to_min;
  uint8_t level;  // RAIN_LIGHT..RAIN_HEAVY
};

// Rainy runs relative to `now`, clipped to the next 60 min. Returns how many were written.
// Slots already in the past fall off as time passes between the 5-min HA updates.
inline size_t rain_runs(const RainForecast &f, int64_t now, RainRun *out, size_t max) {
  if (!f.valid())
    return 0;
  float shift = static_cast<float>(now - f.ref_epoch) / 60.0f;  // minutes since the reference
  size_t n = 0;
  for (size_t i = 0; i < f.count; i++) {
    if (!f.slots[i].rainy())
      continue;
    float from = f.slots[i].start_min - shift;
    float end = (i + 1 < f.count ? f.slots[i + 1].start_min : RainForecast::HORIZON_MIN) - shift;
    if (from < 0)
      from = 0;
    if (end > RainForecast::HORIZON_MIN)
      end = RainForecast::HORIZON_MIN;
    if (end <= from)
      continue;
    if (n > 0 && out[n - 1].to_min >= from && out[n - 1].level == f.slots[i].level) {  // merge same-level neighbours
      out[n - 1].to_min = end;
      continue;
    }
    if (n < max)
      out[n++] = RainRun{from, end, f.slots[i].level};
  }
  return n;
}

// Rain line under the arc (spec.md): "Rain now", "Rain in 15 min", or "" when the hour is dry.
inline void rain_line(const RainRun *runs, size_t n, char *buf, size_t len) {
  if (len == 0)
    return;
  buf[0] = '\0';
  if (n == 0)
    return;
  if (runs[0].from_min < 1.0f) {
    std::snprintf(buf, len, "Rain now");
    return;
  }
  int m = static_cast<int>(runs[0].from_min + 0.5f);
  std::snprintf(buf, len, "Rain in %d min", m);
}

// ---- Hourly forecast (weather.get_forecasts, compacted by response_template; contract.md §6) ----

struct ForecastHour {
  int64_t epoch;      // start of the hour
  char condition[20]; // HA condition string
  float temp;
};

// First row whose hour has not fully passed (the provider's first slot can be the current or the
// next hour; the UI labels the first remaining row "Now"). Returns n when all are past.
inline size_t forecast_first(const ForecastHour *rows, size_t n, int64_t now) {
  size_t i = 0;
  while (i < n && rows[i].epoch + 3600 <= now)
    i++;
  return i;
}

}  // namespace esphome::dial_ui
