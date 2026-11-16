#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if command -v cmake >/dev/null 2>&1; then
  GENERATOR="Unix Makefiles"
  if command -v ninja >/dev/null 2>&1; then GENERATOR="Ninja"; fi
  cmake -S "$ROOT" -B "$ROOT/build" -G "$GENERATOR"
  cmake --build "$ROOT/build"
  ctest --test-dir "$ROOT/build" --output-on-failure
  "$ROOT/build/pc2ps3_demo"
else
  echo "cmake not found; using the minimal host fallback" >&2
  mkdir -p "$ROOT/build-manual"
  g++ -std=c++11 -Wall -Wextra -Wpedantic -I"$ROOT/include" \
    -c "$ROOT/src/core/world.cpp" -o "$ROOT/build-manual/world.o"
  g++ -std=c++11 -Wall -Wextra -Wpedantic -I"$ROOT/include" \
    -c "$ROOT/src/core/render.cpp" -o "$ROOT/build-manual/render.o"
  g++ -std=c++11 -Wall -Wextra -Wpedantic -I"$ROOT/include" \
    "$ROOT/apps/headless/main.cpp" "$ROOT/build-manual/world.o" "$ROOT/build-manual/render.o" \
    -o "$ROOT/build-manual/pc2ps3_demo"
  g++ -std=c++11 -Wall -Wextra -Wpedantic -I"$ROOT/include" \
    "$ROOT/tests/core_test.cpp" "$ROOT/build-manual/world.o" "$ROOT/build-manual/render.o" \
    -o "$ROOT/build-manual/core_test"
  "$ROOT/build-manual/core_test"
  (cd "$ROOT" && "$ROOT/build-manual/pc2ps3_demo")
fi
