#!/bin/sh

set -e

# Define compile job amount
if [ -z "$JOBS" ]; then
    JOBS=1
fi

# Build tidy-html5
cd third_party/tidy-html5/build/cmake
cmake ../.. -DCMAKE_BUILD_TYPE=Release
make -j$JOBS
cd ../../../../

# Build ncurses
cd third_party/ncurses
./configure --enable-widec \
            --without-tack \
            --without-debug \
            --without-progs \
            --without-tests \
            --without-curses-h

make -j$JOBS
cd include/
ln -s curses.h ncurses.h
cd ../../..
