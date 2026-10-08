#!/bin/sh
# Builds DOOM (doomgeneric) for this machine and fetches Freedoom's free IWAD.
# Run once; the mod runs it itself on the first /doom when bin/doom is missing.
set -e
cd "$(dirname "$0")"
mkdir -p bin
if [ ! -d src ]; then
  git clone --depth 1 https://github.com/ozkl/doomgeneric.git src
fi
D=src/doomgeneric
SRCS=$(sed -n 's/^SRC_DOOM = //p' $D/Makefile | tr ' ' '\n' | grep -v doomgeneric_xlib | sed "s|^|$D/|; s|\.o$|.c|")
cc -O2 -w -DNORMALUNIX -D_DEFAULT_SOURCE -I$D -o bin/doom $SRCS doomgeneric_term.c -lm
if [ ! -f bin/freedoom1.wad ]; then
  curl -fsSL -o /tmp/freedoom.zip https://github.com/freedoom/freedoom/releases/download/v0.13.0/freedoom-0.13.0.zip
  unzip -o -j /tmp/freedoom.zip '*/freedoom1.wad' -d bin >/dev/null
  rm -f /tmp/freedoom.zip
fi
echo built
