#!/bin/sh

# Define compile job amount
if [ -z "$JOBS" ]; then
    JOBS=1
fi

# Build tidy-html5
cd third_party/tidy-html5/build/cmake
cmake ../.. -DCMAKE_BUILD_TYPE=Release
make -j$JOBS
