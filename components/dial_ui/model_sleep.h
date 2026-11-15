#pragma once
// Dimming and sleep (spec.md "Behaviour"). Pure C++, host-tested.
// No input for 30 s: dim to 30 %; 60 s in total: screen off. Any input wakes to full brightness
// (from off, that input is not acted on: handled by DialUi). Data and alerts never wake it.

#include <cstdint>

namespace esphome::dial_ui {

enum class Backlight : uint8_t { ACTIVE, DIMMED, OFF };

enum class SleepAction : uint8_t { NONE, DIM, RESTORE, TURN_OFF };

class SleepTimer {
 public:
  SleepTimer(uint32_t dim_after_ms = 30000, uint32_t off_after_ms = 60000)
      : dim_after_(dim_after_ms), off_after_(off_after_ms) {}
  void set_times(uint32_t dim_after_ms, uint32_t off_after_ms) {
    this->dim_after_ = dim_after_ms;
    this->off_after_ = off_after_ms;
  }

  // Called often with the time since the last input. Returns what to do with the backlight.
  // OFF is left only by wake() (an input event), not by this function.
  SleepAction update(uint32_t inactive_ms) {
    switch (this->state_) {
      case Backlight::ACTIVE:
        if (inactive_ms >= this->off_after_) {
          this->state_ = Backlight::OFF;
          return SleepAction::TURN_OFF;
        }
        if (inactive_ms >= this->dim_after_) {
          this->state_ = Backlight::DIMMED;
          return SleepAction::DIM;
        }
        return SleepAction::NONE;
      case Backlight::DIMMED:
        if (inactive_ms < this->dim_after_) {  // input while dimmed: back to full, input acts normally
          this->state_ = Backlight::ACTIVE;
          return SleepAction::RESTORE;
        }
        if (inactive_ms >= this->off_after_) {
          this->state_ = Backlight::OFF;
          return SleepAction::TURN_OFF;
        }
        return SleepAction::NONE;
      default:
        return SleepAction::NONE;
    }
  }

  void wake() { this->state_ = Backlight::ACTIVE; }
  void force_off() { this->state_ = Backlight::OFF; }  // Settings > Screen off / Power off on USB
  Backlight state() const { return this->state_; }

 protected:
  uint32_t dim_after_;
  uint32_t off_after_;
  Backlight state_{Backlight::ACTIVE};
};

// Dim level: 30 % (spec), but never brighter than the user's luminosity.
inline int dim_level(int luminosity) { return luminosity < 30 ? luminosity : 30; }

}  // namespace esphome::dial_ui
