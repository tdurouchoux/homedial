#!/usr/bin/env bash
# Builds and runs the host unit tests for the dial_ui model (no LVGL, no ESPHome).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$here/build"
for t in "$here"/test_*.cpp; do
  name="$(basename "$t" .cpp)"
  g++ -std=c++17 -Wall -Wextra -Werror -I"$here/../../components/dial_ui" "$t" -o "$here/build/$name"
  "$here/build/$name"
done
