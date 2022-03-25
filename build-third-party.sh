#/bin/sh

set -e

# TODO: on MacOS download the dev builds, just like on windows

if [ ! -d "third_party/SDL" ]; then
    curl -L https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.0.20.tar.gz -o third_party/SDL.tar.gz
    cd third_party
    tar -xf SDL.tar.gz
    mv SDL-release-2.0.20 SDL
    rm SDL.tar.gz
    cd SDL
    # 2.0.20 version is not happy to build with wayland so we need to apply
    # a patch from https://github.com/libsdl-org/SDL/pull/5436 before building
    patch -p 1 -i ../sdl-wayland-fix.diff
else
    echo "SDL is already downloaded. remove SDL directory in third_party/ to force redownload"
    cd third_party/SDL
fi

if [ ! -f "./build/.libs/libSDL2.a" ]; then
    # Inspired by debian builds
    # TODO: create a build that only includes things that we actually need (like no audio)
    #       so we can create a small program and reduce the memory footprint
    ./configure \
        --build=x86_64-linux-gnu \
        --disable-option-checking \
        --disable-silent-rules \
        --disable-maintainer-mode \
        --disable-dependency-tracking \
        --disable-rpath \
        --disable-nas \
        --disable-esd \
        --disable-arts \
        --disable-alsa-shared \
        --disable-pulseaudio-shared \
        --enable-ibus \
        --disable-x11-shared \
        --disable-video-directfb \
        --enable-video-opengles \
        --disable-video-opengles1 \
        --enable-video-kmsdrm \
        --disable-kmsdrm-shared \
        --enable-hidapi \
        --enable-hidapi-joystick \
        --enable-libdecor \
        --enable-pipewire \
        --enable-video-vulkan \
        --disable-wayland-shared \
        --enable-video-wayland ac_cv_header_libunwind_h=no
    make -j$(nproc)
else
    echo "SDL is already built. Run make clean in third_party/SDL to force rebuild"
fi

cd ../..

if [ ! -d "third_party/SDL_ttf" ]; then
    curl -L https://github.com/libsdl-org/SDL_ttf/archive/refs/tags/release-2.0.18.tar.gz -o third_party/SDL_ttf.tar.gz
    cd third_party
    tar -xf SDL_ttf.tar.gz
    mv SDL_ttf-release-2.0.18 SDL_ttf
    rm SDL_ttf.tar.gz
    cd SDL_ttf
else
    echo "SDL_ttf is already downloaded. remove SDL_ttf directory in third_party/ to force redownload"
    cd third_party/SDL_ttf
fi

if [ ! -f "./.libs/libSDL2_ttf.a" ]; then
    ./configure \
        --build=x86_64-linux-gnu \
        --disable-option-checking \
        --disable-silent-rules \
        --disable-maintainer-mode \
        --disable-dependency-tracking \
        --enable-harfbuzz \
        --disable-freetype-builtin
        make -j$(nproc)
    cp SDL_ttf.h ../SDL/include
fi

cd ../..
