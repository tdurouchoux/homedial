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

  DetentAccel a;
  CHECK(a.steps(1000, 1) == 1);
  CHECK(a.steps(1050, 1) == 1 && a.steps(1100, 1) == 1 && a.steps(1150, 1) == 1);
  CHECK(a.steps(1200, 1) == 2);  // fifth fast detent: accelerated
  CHECK(a.steps(1250, -1) == 1);  // direction change resets
  CHECK(a.steps(2000, -1) == 1);  // slow resets

  if (failures == 0)
    std::printf("All settings tests passed\n");
  return failures == 0 ? 0 : 1;
}
