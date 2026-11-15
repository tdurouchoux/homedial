#!/usr/bin/env bash
# Builds and runs the host unit tests for the dial_ui model (no LVGL, no ESPHome).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$here/build"
g++ -std=c++17 -Wall -Wextra -Werror -I"$here/../../components/dial_ui" \
  "$here/test_model.cpp" -o "$here/build/test_model"
"$here/build/test_model"
