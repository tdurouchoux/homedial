// Host unit tests for model_power.h.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "model_power.h"

using namespace esphome::dial_ui;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                 \
    }                                                             \
  } while (0)

static bool fmt(float w, bool online, const char *want_v, const char *want_u) {
  char v[16];
  const char *u;
  format_power(w, online, v, sizeof v, &u);
  bool ok = std::strcmp(v, want_v) == 0 && std::strcmp(u, want_u) == 0;
  if (!ok)
    std::printf("  format_power(%g) = '%s' %s\n", w, v, u);
  return ok;
}

int main() {
  const float L = 6000;
  CHECK(power_status(1240, L, true) == Status::OK);
  CHECK(power_status(4799, L, true) == Status::OK);
  CHECK(power_status(4800, L, true) == Status::WARN);   // 80 %
  CHECK(power_status(5999, L, true) == Status::WARN);
  CHECK(power_status(6000, L, true) == Status::ALERT);  // 100 %
  CHECK(power_status(NAN, L, true) == Status::OFFLINE);
  CHECK(power_status(100, L, false) == Status::OFFLINE);
  CHECK(std::fabs(power_fraction(1500, L) - 0.25f) < 1e-6f);
  CHECK(power_fraction(9000, L) == 1.0f);  // full at and above the limit
  CHECK(power_fraction(-200, L) == 0.0f);  // export
  CHECK(power_fraction(NAN, L) == 0.0f);
  CHECK(fmt(1240.4f, true, "1240", "W"));
  CHECK(fmt(9999, true, "9999", "W"));
  CHECK(fmt(12340, true, "12.3", "kW"));
  CHECK(fmt(-350, true, "-350", "W"));
  CHECK(fmt(NAN, true, "--", "W"));
  CHECK(fmt(500, false, "--", "W"));
  if (failures == 0)
    std::printf("All power tests passed\n");
  return failures == 0 ? 0 : 1;
}
