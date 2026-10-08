#!/bin/bash
# Hear Jerry's boot grooves: renders one .wav per intro into docs/sounds/ (Linux/WSL, needs g++).
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$REPO/docs/sounds"
g++ -std=gnu++17 -O2 -Wall -Wextra -I "$REPO/src" "$REPO/tools/sim/render_funk.cpp" \
  "$REPO/src/funk.cpp" -o /tmp/jerryos_render_funk
/tmp/jerryos_render_funk "$REPO/docs/sounds"
