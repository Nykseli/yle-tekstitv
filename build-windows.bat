@echo off
setlocal enabledelayedexpansion

:: This requires visual studio tools with clang support
:: Only the x86 Native Tools Command Propmt for VS is supported
:: See more in https://docs.microsoft.com/en-us/cpp/build/walkthrough-compiling-a-native-cpp-program-on-the-command-line?view=msvc-170

if not exist tekstitv (
    mkdir tekstitv
    copy third_party\curl\builds\libcurl\bin\libcurl.dll tekstitv\libcurl.dll
    copy third_party\SDL\lib\x86\SDL2.dll tekstitv\SDL2.dll
    copy third_party\SDL_ttf\lib\x86\SDL2_ttf.dll tekstitv\SDL2_ttf.dll
    mkdir tekstitv\assets
    xcopy assets tekstitv\assets /E
)

nmake /F Makefile.vc GUI_ENABLED=true
