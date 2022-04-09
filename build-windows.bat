@echo off
setlocal enabledelayedexpansion

set imgui_files=third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp third_party\imgui\backends\imgui_impl_sdlrenderer.cpp third_party\imgui\backends\imgui_impl_sdl.cpp
set imgui_o_files=third_party\imgui\imgui.o third_party\imgui\imgui_draw.o third_party\imgui\imgui_tables.o third_party\imgui\imgui_widgets.o third_party\imgui\backends\imgui_impl_sdlrenderer.o third_party\imgui\backends\imgui_impl_sdl.o
set C_FILES=src/config.c src/main.c src/printer.c lib/html_parser.c lib/page_loader.c
set C_O_FILES=src/config.o src/main.o src/printer.o lib/html_parser.o lib/page_loader.o
set CXX_FILES=src/gui.cpp
set CFLAGS=-std=c99 -O3 -Wall -Wextra -Werror -Wno-unused-parameter -Wformat-security -Wno-unused-result -pedantic -fPIC -DENABLE_GUI
set CXXFLAGS=-std=c++11 -O3 -Wall -Wextra -Wno-unused-parameter -Wformat-security -Wno-unused-result -pedantic -fPIC -DENABLE_GUI -mwindows
set SDL_LINKS=-Lthird_party/SDL/i686-w64-mingw32/lib -lmingw32 -lSDL2main -lSDL2 -Lthird_party/SDL_ttf/i686-w64-mingw32/lib -lSDL2_ttf
set CURL_LINKS=-Lthird_party/curl/lib -lcurl
set INCLUDES=-Iinclude -Ithird_party -Ithird_party/imgui -Ithird_party/SDL/include/SDL2 -Ithird_party/curl/include


(for %%f in (%imgui_files%) do (
    set file=%%f
    set o_file=!file:cpp=o!
    if not exist !o_file! (
        echo Compiling !o_file!
        g++ %INCLUDES% -c %CXXFLAGS% -o !o_file! !file!
    )
))

(for %%f in (%C_FILES%) do (
    set file=%%f
    set o_file=!file:.c=.o!
    :: batch files doesn't have the or operator so here we are
    if not exist !o_file! (
        echo Compiling !o_file!
        gcc %INCLUDES% -c %CFLAGS% -o !o_file! !file!
    ) else if [%1] == [clean] (
        echo Compiling !o_file!
        gcc %INCLUDES% -c %CFLAGS% -o !o_file! !file!
    )
))

if not exist tekstitv (
    mkdir tekstitv
    copy third_party\curl\bin\libcurl.dll tekstitv\libcurl.dll
    copy third_party\SDL\i686-w64-mingw32\bin\SDL2.dll tekstitv\SDL2.dll
    copy third_party\SDL_ttf\i686-w64-mingw32\bin\SDL2_ttf.dll tekstitv\SDL2_ttf.dll
    mkdir tekstitv\assets
    xcopy assets tekstitv\assets /E
)

g++ %INCLUDES% %CXXFLAGS% -mwindows %CXX_FILES% -c -o src/gui.o
g++ -v %INCLUDES% %CFLAGS% %imgui_o_files% %C_O_FILES% src/gui.o %SDL_LINKS% %CURL_LINKS% -o tekstitv/tekstitv.exe

