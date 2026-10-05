#!/bin/bash
# Unit tests for tracker detection + persistence (Linux / WSL, needs g++).
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
g++ -std=gnu++17 -Wall -Wextra -fsanitize=address,undefined -g -I "$REPO/src" \
  "$REPO/tools/sim/test_trackers.cpp" "$REPO/src/tracker_detect.cpp" "$REPO/src/tracker_watch.cpp" \
  -o /tmp/jerryos_test_trackers
/tmp/jerryos_test_trackers
