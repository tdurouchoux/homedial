// Host unit tests for model_weather.h.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "model_weather.h"

using namespace esphome::dial_ui;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                 \
    }                                                             \
  } while (0)

static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

int main() {
  // Conditions: all 15 HA strings map; words fit in 12 characters.
  const char *ALL[] = {"clear-night", "cloudy", "exceptional", "fog", "hail", "lightning", "lightning-rainy",
                       "partlycloudy", "pouring", "rainy", "snowy", "snowy-rainy", "sunny", "windy", "windy-variant"};
  for (const char *c : ALL) {
    ConditionInfo ci;
    CHECK(condition_info(c, &ci));
    CHECK(std::strlen(ci.word) <= 12);
  }
  ConditionInfo ci;
  CHECK(condition_info("pouring", &ci) && ci.icon == WeatherIcon::RAIN);
  CHECK(condition_info("hail", &ci) && ci.icon == WeatherIcon::SNOW);
  CHECK(condition_info("windy-variant", &ci) && ci.icon == WeatherIcon::PART_CLOUDY);
  CHECK(!condition_info("unavailable", &ci) && !condition_info(nullptr, &ci));

  // ISO 8601 as HA sends it.
  int64_t t;
  CHECK(parse_iso8601("1970-01-01T00:00:00+00:00", &t) && t == 0);
  CHECK(parse_iso8601("2025-11-15T10:00:00+00:00", &t) && t == 1763200800);
  CHECK(parse_iso8601("2025-11-15T11:00:00+01:00", &t) && t == 1763200800);
  CHECK(parse_iso8601("2025-11-15T10:00:00Z", &t) && t == 1763200800);
  CHECK(parse_iso8601("2025-11-15T10:00:00.123456+00:00", &t) && t == 1763200800);
  CHECK(!parse_iso8601("unknown", &t) && !parse_iso8601(nullptr, &t));

  // Météo-France 1_hour_forecast, as str(dict).
  RainForecast f;
  const char *ATTR =
      "{'0 min': 'Temps sec', '5 min': 'Temps sec', '10 min': 'Pluie faible', '15 min': 'Pluie mod\xC3\xA9r\xC3\xA9" "e', "
      "'20 min': 'Temps sec', '25 min': 'Temps sec', '30 min': 'Temps sec', '40 min': 'Pluie forte', '50 min': 'Temps sec'}";
  CHECK(parse_rain_slots(ATTR, &f) && f.count == 9);
  CHECK(f.slots[0].level == RAIN_DRY && f.slots[2].level == RAIN_LIGHT && f.slots[3].level == RAIN_MODERATE &&
        f.slots[7].level == RAIN_HEAVY && f.slots[8].level == RAIN_DRY);
  CHECK(parse_rain_slots("{\"0 min\": \"Pluie d'orage\", '5 min': 'Temps sec'}", &f) &&
        f.slots[0].level == RAIN_MODERATE);
  CHECK(!parse_rain_slots("unknown", &f) && !parse_rain_slots("{'x': 'y'}", &f) && !parse_rain_slots("{}", &f));

  // Runs relative to now, one per level: light 10-15, moderate 15-20, heavy 40-50 min.
  parse_rain_slots(ATTR, &f);
  f.ref_epoch = 1763200800;
  RainRun runs[8];
  size_t n = rain_runs(f, f.ref_epoch, runs, 8);
  CHECK(n == 3 && near(runs[0].from_min, 10) && near(runs[0].to_min, 15) && runs[0].level == RAIN_LIGHT &&
        near(runs[1].from_min, 15) && near(runs[1].to_min, 20) && runs[1].level == RAIN_MODERATE &&
        near(runs[2].from_min, 40) && near(runs[2].to_min, 50) && runs[2].level == RAIN_HEAVY);
  char line[24];
  rain_line(runs, n, line, sizeof line);
  CHECK(std::strcmp(line, "Rain in 10 min") == 0);
  // 12 minutes later the first run has started: "Rain now", shifted runs.
  n = rain_runs(f, f.ref_epoch + 12 * 60, runs, 8);
  CHECK(n == 3 && near(runs[0].from_min, 0) && near(runs[0].to_min, 3) && near(runs[1].to_min, 8));
  rain_line(runs, n, line, sizeof line);
  CHECK(std::strcmp(line, "Rain now") == 0);
  // An hour later everything is past: dry, no line.
  n = rain_runs(f, f.ref_epoch + 3600, runs, 8);
  rain_line(runs, n, line, sizeof line);
  CHECK(n == 0 && line[0] == '\0');
  // Same-level neighbours merge into one run.
  RainForecast g;
  parse_rain_slots("{'0 min': 'Pluie faible', '5 min': 'Pluie faible', '10 min': 'Temps sec'}", &g);
  g.ref_epoch = 0;
  CHECK(rain_runs(g, 0, runs, 8) == 1 && near(runs[0].to_min, 10));
  // Not loaded yet.
  RainForecast empty;
  CHECK(rain_runs(empty, 0, runs, 8) == 0);
  // Last slot runs to the 60 min horizon.
  parse_rain_slots("{'0 min': 'Temps sec', '50 min': 'Pluie faible'}", &f);
  f.ref_epoch = 0;
  n = rain_runs(f, 0, runs, 8);
  CHECK(n == 1 && near(runs[0].from_min, 50) && near(runs[0].to_min, 60));

  // Forecast rows: hours already past are skipped.
  ForecastHour rows[3] = {{1000 * 3600, "sunny", 10}, {1001 * 3600, "rainy", 11}, {1002 * 3600, "cloudy", 12}};
  CHECK(forecast_first(rows, 3, 1000 * 3600 + 1800) == 0);  // inside the first hour
  CHECK(forecast_first(rows, 3, 1001 * 3600) == 1);         // first hour fully past
  CHECK(forecast_first(rows, 3, 1003 * 3600) == 3);         // all past

  if (failures == 0)
    std::printf("All weather tests passed\n");
  return failures == 0 ? 0 : 1;
}
