#!/bin/sh

set -e

# Define compile job amount
if [ -z "$JOBS" ]; then
    JOBS=1
fi


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

# Build static curl
cd third_party/curl
./buildconf
./configure --disable-ipv6 \
            --disable-debug \
            --disable-shared \
            --disable-thread \
            --disable-cookies \
            --enable-optimize \
            --disable-verbose \
            --disable-pthreads \
            --enable-hidden-symbols \
            --disable-threaded-resolver
make -j$JOBS
cd ../..
