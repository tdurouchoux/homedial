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
  CHECK(pitch.def == 1000 && setting_positions(pitch) == 18);
  CHECK(setting_at(pitch, 0) == 100 && setting_at(pitch, 17) == 6000 && setting_at(pitch, 99) == 6000);
  CHECK(clamp_setting(pitch, 9000) == 6000 && clamp_setting(pitch, 1300) == 1200 && clamp_setting(pitch, 50) == 100);
  CHECK(setting_step(pitch, 1000, 1) == 1200 && setting_step(pitch, 1000, -2) == 600);
  CHECK(setting_step(pitch, 6000, 1) == 6000 && setting_step(pitch, 100, -1) == 100);  // clamped at the ends
  for (size_t i = 1; i < setting_positions(pitch); i++)
    CHECK(setting_at(pitch, i) > setting_at(pitch, i - 1));
  // Linear specs keep their min/step grid.
  CHECK(setting_positions(lum) == 19 && setting_at(lum, 0) == 10 && setting_index(lum, 55) == 9);
  CHECK(setting_step(lum, 95, 2) == 100 && setting_step(vol, 50, -1) == 40);
  CHECK(std::strcmp(lum.unit, "%") == 0 && std::strcmp(vol.unit, "%") == 0);
  char v[8];
  std::snprintf(v, sizeof v, "%d%s", setting_at(pitch, setting_positions(pitch) - 1), pitch.unit);
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
