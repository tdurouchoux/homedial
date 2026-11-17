#pragma once
// Settings model (spec.md "Settings list"). Pure C++, host-tested.

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace esphome::dial_ui {

// BUZZER_PITCH is deactivated for now (the buzzer plays a fixed tone per interaction, see
// dial_ui.cpp EVENT_TONES): it sits after the listed rows, so the list has SETTINGS_ROWS rows and
// the Pitch row never shows. To bring it back, move it between BUZZER and SCREEN_OFF again,
// set SETTINGS_ROWS to SETTINGS_ENUM_COUNT, and set EVENT_TONES to false.
enum class SettingsRow : uint8_t { LUMINOSITY, BUZZER, SCREEN_OFF, POWER_OFF, EXIT, BUZZER_PITCH };
constexpr size_t SETTINGS_ENUM_COUNT = 6;  // every row, listed or not
constexpr size_t SETTINGS_ROWS = 5;        // rows shown in the list (BUZZER_PITCH is not)

inline const char *settings_label(SettingsRow r) {
  // "Pitch", not "Buzzer pitch": with its value ("6000Hz") it must fit the 144 px inside a row (user).
  static const char *const L[SETTINGS_ENUM_COUNT] = {"Luminosity", "Buzzer vol.", "Screen off", "Power off", "Exit", "Pitch"};
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
  // Optional fixed list of values (ascending). When set, min/max/step are ignored and one
  // detent moves one entry: used for pitch, where equal Hz steps do not sound equal.
  const int *values{nullptr};
  size_t count{0};
};

inline const AdjustSpec &adjust_spec(SettingsRow r) {
  // Luminosity never reaches 0 from Settings ("Screen off" does that).
  static const AdjustSpec LUM{"Luminosity", 10, 100, 5, 100, "%"};
  static const AdjustSpec VOL{"Buzzer volume", 0, 100, 10, 50, "%"};
  // Tick pitch, 100 Hz to 6 kHz (user: above 6 kHz is unbearable, more range wanted at the low end),
  // spaced roughly evenly to the ear. 1 kHz (the LEDC default) keeps the original sound.
  static const int PITCHES[] = {100,  150,  200,  250,  300,  400,  500,  600,  800,
                                1000, 1200, 1500, 2000, 2500, 3000, 4000, 5000, 6000};
  static const AdjustSpec PITCH{"Buzzer pitch", 100, 6000, 0, 1000, "Hz", PITCHES, sizeof(PITCHES) / sizeof(int)};
  if (r == SettingsRow::BUZZER)
    return VOL;
  if (r == SettingsRow::BUZZER_PITCH)
    return PITCH;
  return LUM;
}

inline bool is_adjustable(SettingsRow r) {
  return r == SettingsRow::LUMINOSITY || r == SettingsRow::BUZZER || r == SettingsRow::BUZZER_PITCH;
}

// Number of positions on the Adjust ring, and the value at / index of a position.
inline size_t setting_positions(const AdjustSpec &s) {
  return s.values != nullptr ? s.count : static_cast<size_t>((s.max - s.min) / s.step + 1);
}
inline int setting_at(const AdjustSpec &s, size_t i) {
  size_t n = setting_positions(s);
  if (i >= n)
    i = n - 1;
  return s.values != nullptr ? s.values[i] : s.min + static_cast<int>(i) * s.step;
}
inline size_t setting_index(const AdjustSpec &s, int v) {
  if (s.values == nullptr)
    return static_cast<size_t>((v - s.min) / s.step);
  size_t best = 0;
  for (size_t i = 1; i < s.count; i++) {
    int d = s.values[i] - v, db = s.values[best] - v;
    if ((d < 0 ? -d : d) < (db < 0 ? -db : db))
      best = i;
  }
  return best;
}

inline int clamp_setting(const AdjustSpec &s, int v) {
  if (s.values != nullptr)
    return s.values[setting_index(s, v)];  // nearest listed value
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

// Moves `v` by `steps` positions (one detent = one position), clamped to the ends.
inline int setting_step(const AdjustSpec &s, int v, int steps) {
  int i = static_cast<int>(setting_index(s, clamp_setting(s, v))) + steps;
  int n = static_cast<int>(setting_positions(s));
  if (i < 0)
    i = 0;
  if (i >= n)
    i = n - 1;
  return setting_at(s, static_cast<size_t>(i));
}

}  // namespace esphome::dial_ui
