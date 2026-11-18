#!/bin/sh
# Render PNG screenshots of each state with the SHOTS developer build under Wine.
set -eu
cd "$(dirname "$0")/.."
export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-kt} WINEDEBUG=-all
mkdir -p shots
if [ ! -f shots/bg.png ]; then
  python3 tools/compose_docs.py
fi
for s in "$@"; do
  xvfb-run -a wine build/KeypadTimer-shots.exe --shot "Z:$(pwd | tr / '\\')\\shots\\$s.png" "$s" 2>/dev/null
done
python3 tools/compose_docs.py "$@"
