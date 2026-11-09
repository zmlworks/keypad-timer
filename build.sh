#!/bin/sh
# Keypad Timer - Windows x64 build with MinGW-w64 (Debian/Ubuntu: apt install gcc-mingw-w64-x86-64)
set -eu
cd "$(dirname "$0")"
mkdir -p build
CC=x86_64-w64-mingw32-gcc
CFLAGS="-Os -Wall -Wextra -Wno-missing-field-initializers -municode -mwindows -s -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables"
LIBS="-lgdiplus -lwinmm -lcomctl32 -lshell32 -lgdi32 -luser32 -lkernel32"
x86_64-w64-mingw32-windres -O coff -I src -i src/app.rc -o build/app.res.o
$CC $CFLAGS -Wl,--gc-sections -static-libgcc -o build/KeypadTimer.exe src/main.c src/tlogic.c build/app.res.o $LIBS
if [ "${SHOTS:-0}" = 1 ]; then
  $CC $CFLAGS -DSHOTS -Wl,--gc-sections -static-libgcc -o build/KeypadTimer-shots.exe src/main.c src/tlogic.c build/app.res.o $LIBS
fi
rm -f build/app.res.o
ls -l build/*.exe
