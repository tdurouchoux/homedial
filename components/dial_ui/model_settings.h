#pragma once
// Settings model (spec.md "Settings list"). Pure C++, host-tested.

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace esphome::dial_ui {

enum class SettingsRow : uint8_t { LUMINOSITY, BUZZER, BUZZER_PITCH, SCREEN_OFF, POWER_OFF, EXIT };
constexpr size_t SETTINGS_ROWS = 6;

inline const char *settings_label(SettingsRow r) {
  static const char *const L[SETTINGS_ROWS] = {"Luminosity", "Buzzer vol.", "Buzzer pitch", "Screen off", "Power off",
                                                "Exit"};
  return L[static_cast<size_t>(r)];
}

// An adjustable value (ScreenAdjust): range, step and default (spec.md, decided 2026-11-15).
struct AdjustSpec {
  const char *name;  // shown on the Adjust page
  int min;
  int max;
  int step;
  int def;
  const char *unit;  // "%" or "Hz"
};

inline const AdjustSpec &adjust_spec(SettingsRow r) {
  // Luminosity never reaches 0 from Settings ("Screen off" does that).
  static const AdjustSpec LUM{"Luminosity", 10, 100, 5, 100, "%"};
  static const AdjustSpec VOL{"Buzzer volume", 0, 100, 10, 50, "%"};
  // Tick pitch. M5Stack's own Dial examples tick at 8 kHz (M5Dial encoder.ino: Speaker.tone(8000, 20)),
  // so the range reaches 8 kHz; 1 kHz (the LEDC default) keeps the original sound.
  static const AdjustSpec PITCH{"Buzzer pitch", 500, 8000, 500, 1000, "Hz"};
  if (r == SettingsRow::BUZZER)
    return VOL;
  if (r == SettingsRow::BUZZER_PITCH)
    return PITCH;
  return LUM;
}

inline bool is_adjustable(SettingsRow r) {
  return r == SettingsRow::LUMINOSITY || r == SettingsRow::BUZZER || r == SettingsRow::BUZZER_PITCH;
}

inline int clamp_setting(const AdjustSpec &s, int v) {
  if (v < s.min)
    return s.min;
  if (v > s.max)
    return s.max;
  // Snap onto the step grid from min.
  return s.min + ((v - s.min + s.step / 2) / s.step) * s.step;
}

// One detent = one step; after 4 fast detents (< 120 ms apart) the step doubles (design ScreenAdjust).
class DetentAccel {
 public:
  static constexpr uint32_t FAST_MS = 120;
  int steps(uint32_t now_ms, int dir) {
    if (dir != this->last_dir_ || now_ms - this->last_ms_ > FAST_MS)
      this->run_ = 0;
    this->run_++;
    this->last_ms_ = now_ms;
    this->last_dir_ = dir;
    return this->run_ > 4 ? 2 : 1;
  }

 protected:
  uint32_t last_ms_{0};
  int last_dir_{0};
  int run_{0};
};

// Buzzer volume (0..100 %) -> PWM duty (0..0.5). A passive buzzer gets only slightly quieter as the
// duty drops from 50 % to ~10 %; the audible change happens at very low duty. A cubic curve spreads
// the steps where loudness actually changes (a linear map made most steps sound the same; user).
inline float buzzer_level(int volume_percent) {
  if (volume_percent <= 0)
    return 0.0f;
  float v = volume_percent > 100 ? 1.0f : static_cast<float>(volume_percent) / 100.0f;
  return 0.5f * v * v * v;
}

}  // namespace esphome::dial_ui
