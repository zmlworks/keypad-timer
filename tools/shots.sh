#!/bin/sh
# Render PNG screenshots of each state with the SHOTS developer build under Wine.
set -eu
cd "$(dirname "$0")/.."
export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-kt} WINEDEBUG=-all
mkdir -p shots
for s in "$@"; do
  xvfb-run -a wine build/KeypadTimer-shots.exe --shot "Z:$(pwd | tr / '\\')\\shots\\$s.png" "$s" 2 2>/dev/null
  convert shots/bg.png shots/$s.png -geometry +20+20 -composite shots/c_$s.png
done
