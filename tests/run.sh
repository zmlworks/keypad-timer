#!/bin/sh
# Unit tests for the timer logic and generated sounds (runs on Linux/macOS with a C compiler).
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
cc -Wall -Wextra -O2 -o build/test_tlogic tests/test_tlogic.c src/tlogic.c -lm
cd build && ./test_tlogic
