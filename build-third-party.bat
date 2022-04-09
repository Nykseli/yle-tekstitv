@echo off

set start_path=%cd%

cd third_party

if not exist SDL (
    curl -L https://github.com/libsdl-org/SDL/releases/download/release-2.0.20/SDL2-devel-2.0.20-mingw.tar.gz -o SDL.tar.gz
    tar xf SDL.tar.gz
    move SDL2-2.0.20 SDL
    cd SDL
    move i686-w64-mingw32\include include
    cd ..
    del SDL.tar.gz
)

if not exist SDL_ttf (
    curl -L https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.0.18/SDL2_ttf-devel-2.0.18-mingw.tar.gz -o SDL_ttf.tar.gz
    tar xf SDL_ttf.tar.gz
    move SDL2_ttf-2.0.18 SDL_ttf
    cd SDL_ttf
    copy i686-w64-mingw32\include\SDL2\SDL_ttf.h ..\SDL\include\SDL2\SDL_ttf.h
    cd ..
    del SDL_ttf.tar.gz
)

if not exist curl (
    curl -L https://curl.se/windows/dl-7.82.0_4/curl-7.82.0_4-win32-mingw.zip -o curl.zip
    tar -xf curl.zip
    move curl-7.82.0-win32-mingw curl
    del curl.zip
)

cd %start_path%
