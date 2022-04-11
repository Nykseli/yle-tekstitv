@echo off
setlocal enabledelayedexpansion

:: This requires visual studio tools with clang support
:: Only the x86 Native Tools Command Propmt for VS is supported
:: See more in https://docs.microsoft.com/en-us/cpp/build/walkthrough-compiling-a-native-cpp-program-on-the-command-line?view=msvc-170

set imgui_files=third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp third_party\imgui\backends\imgui_impl_sdlrenderer.cpp third_party\imgui\backends\imgui_impl_sdl.cpp
set imgui_o_files=third_party\imgui\imgui.o third_party\imgui\imgui_draw.o third_party\imgui\imgui_tables.o third_party\imgui\imgui_widgets.o third_party\imgui\backends\imgui_impl_sdlrenderer.o third_party\imgui\backends\imgui_impl_sdl.o
set C_FILES=src/config.c src/main.c src/printer.c lib/html_parser.c lib/page_loader.c
set C_O_FILES=src/config.o src/main.o src/printer.o lib/html_parser.o lib/page_loader.o
set CXX_FILES=src/gui.cpp
:: TODO: add -Werror to CFLAGS and CXX flags
set CFLAGS=-std=c99 -O3 -Wall -Wextra -Wno-unused-parameter -Wformat-security -Wno-unused-result -pedantic -DENABLE_GUI
set CXXFLAGS=-std=c++11 -O3 -Wall -Wextra -Wno-unused-parameter -Wformat-security -Wno-unused-result -pedantic -DENABLE_GUI
set SDL_LINKS=-Lthird_party/SDL/lib/x86 -lSDL2main -lSDL2 -Lthird_party/SDL_ttf/lib/x86 -lSDL2_ttf
set CURL_LINKS=-Lthird_party/curl/builds/libcurl/lib -llibcurl
set INCLUDES=-Iinclude -Ithird_party -Ithird_party/imgui -Ithird_party/SDL/include -Ithird_party/curl/builds/libcurl/include


(for %%f in (%imgui_files%) do (
    set file=%%f
    set o_file=!file:cpp=o!
    :: batch files doesn't have the or operator so here we are
    if not exist !o_file! (
        echo Compiling !o_file!
        clang++ %INCLUDES% -c %CXXFLAGS% -o !o_file! !file!
    ) else if [%1] == [clean] (
        echo Compiling !o_file!
        clang++ %INCLUDES% -c %CXXFLAGS% -o !o_file! !file!
    )
))

(for %%f in (%C_FILES%) do (
    set file=%%f
    set o_file=!file:.c=.o!
    :: batch files doesn't have the or operator so here we are
    if not exist !o_file! (
        echo Compiling !o_file!
        clang %INCLUDES% -c %CFLAGS% -o !o_file! !file!
    ) else if [%1] == [clean] (
        echo Compiling !o_file!
        clang %INCLUDES% -c %CFLAGS% -o !o_file! !file!
    )
))

if not exist tekstitv (
    mkdir tekstitv
    copy third_party\curl\builds\libcurl\bin\libcurl.dll tekstitv\libcurl.dll
    copy third_party\SDL\lib\x86\SDL2.dll tekstitv\SDL2.dll
    copy third_party\SDL_ttf\lib\x86\SDL2_ttf.dll tekstitv\SDL2_ttf.dll
    mkdir tekstitv\assets
    xcopy assets tekstitv\assets /E
)

clang++ %INCLUDES% %CXXFLAGS% %CXX_FILES% -c -o src/gui.o
clang++ %INCLUDES% %CFLAGS% %imgui_o_files% %C_O_FILES% src/gui.o -static %SDL_LINKS% %CURL_LINKS% -o tekstitv/tekstitv.exe
