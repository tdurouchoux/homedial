// Host unit tests for model_settings.h.
#include <cstdio>
#include <cstring>

#include "model_settings.h"

using namespace esphome::dial_ui;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                 \
    }                                                             \
  } while (0)

int main() {
  for (size_t i = 0; i < SETTINGS_ROWS; i++)
    CHECK(std::strlen(settings_label(static_cast<SettingsRow>(i))) <= 12);  // spec: 12 characters or fewer
  const AdjustSpec &lum = adjust_spec(SettingsRow::LUMINOSITY);
  const AdjustSpec &vol = adjust_spec(SettingsRow::BUZZER);
  CHECK(lum.min == 10 && lum.max == 100 && lum.step == 5 && lum.def == 100);
  CHECK(vol.min == 0 && vol.max == 100 && vol.step == 10 && vol.def == 50);
  CHECK(clamp_setting(lum, 0) == 10 && clamp_setting(lum, 120) == 100 && clamp_setting(lum, 47) == 45);
  CHECK(clamp_setting(lum, 48) == 50 && clamp_setting(vol, 55) == 60 && clamp_setting(vol, -3) == 0);
  CHECK(is_adjustable(SettingsRow::BUZZER) && !is_adjustable(SettingsRow::EXIT));
  const AdjustSpec &pitch = adjust_spec(SettingsRow::BUZZER_PITCH);
  CHECK(is_adjustable(SettingsRow::BUZZER_PITCH) && std::strcmp(pitch.unit, "Hz") == 0);
  CHECK(pitch.min == 500 && pitch.max == 8000 && pitch.step == 500 && pitch.def == 1000);
  CHECK(clamp_setting(pitch, 9000) == 8000 && clamp_setting(pitch, 1300) == 1500);
  CHECK(std::strcmp(lum.unit, "%") == 0 && std::strcmp(vol.unit, "%") == 0);
  char v[8];
  std::snprintf(v, sizeof v, "%d%s", pitch.max, pitch.unit);
  CHECK(std::strlen(v) <= 6);  // ListMenu: a row value is at most 6 characters

  DetentAccel a;
  CHECK(a.steps(1000, 1) == 1);
  CHECK(a.steps(1050, 1) == 1 && a.steps(1100, 1) == 1 && a.steps(1150, 1) == 1);
  CHECK(a.steps(1200, 1) == 2);  // fifth fast detent: accelerated
  CHECK(a.steps(1250, -1) == 1);  // direction change resets
  CHECK(a.steps(2000, -1) == 1);  // slow resets

  // Volume curve: silent at 0, 50 % duty (loudest) at 100, steep at the low end.
  CHECK(buzzer_level(0) == 0.0f && buzzer_level(-5) == 0.0f);
  CHECK(buzzer_level(100) == 0.5f && buzzer_level(150) == 0.5f);
  CHECK(buzzer_level(50) > 0.06f && buzzer_level(50) < 0.065f);
  CHECK(buzzer_level(10) > 0.0f && buzzer_level(10) < 0.001f);
  for (int v = 10; v <= 100; v += 10)
    CHECK(buzzer_level(v) > buzzer_level(v - 10));  // every step is a change

  if (failures == 0)
    std::printf("All settings tests passed\n");
  return failures == 0 ? 0 : 1;
}
