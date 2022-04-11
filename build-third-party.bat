@echo off

:: This requires visual studio tools with clang support
:: Only the x86 Native Tools Command Propmt for VS is supported
:: See more in https://docs.microsoft.com/en-us/cpp/build/walkthrough-compiling-a-native-cpp-program-on-the-command-line?view=msvc-170

set start_path=%cd%

cd third_party

if not exist SDL (
    curl -L https://github.com/libsdl-org/SDL/releases/download/release-2.0.20/SDL2-devel-2.0.20-VC.zip -o SDL.zip
    tar xf SDL.zip
    move SDL2-2.0.20 SDL
    del SDL.zip
)

if not exist SDL_ttf (
    curl -L https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.0.18/SDL2_ttf-devel-2.0.18-VC.zip -o SDL_ttf.zip
    tar xf SDL_ttf.zip
    move SDL2_ttf-2.0.18 SDL_ttf
    cd SDL_ttf
    copy include\SDL_ttf.h ..\SDL\include\SDL_ttf.h
    cd ..
    del SDL_ttf.zip
)

if not exist curl (
    curl -L https://github.com/curl/curl/releases/download/curl-7_82_0/curl-7.82.0.tar.gz -o curl.tar.gz
    tar -xf curl.tar.gz
    move curl-7.82.0 curl
    del curl.tar.gz
    set party_path=%cd%
    cd curl\winbuild\
    nmake /f Makefile.vc mode=dll
    cd ..
    move builds/libcurl-vc-x86-release-dll-ipv6-sspi-schannel builds/libcurl
    cd %party_path%
)

cd %start_path%
