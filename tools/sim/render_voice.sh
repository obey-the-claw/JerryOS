#!/bin/bash
# Hear Jerry: renders every sound to docs/sounds/jerry_<name>.wav (Linux/WSL, needs g++).
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$REPO/docs/sounds"
g++ -std=gnu++17 -O1 -Wall -Wextra -I "$REPO/src" "$REPO/tools/sim/render_voice.cpp" \
  "$REPO/src/squatch_voice.cpp" -o /tmp/jerryos_render_voice
/tmp/jerryos_render_voice "$REPO/docs/sounds"
