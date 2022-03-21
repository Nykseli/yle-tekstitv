@echo off

set C_FILES=src/config.c src/main.c src/printer.c lib/html_parser.c lib/page_loader.c
set CXX_FILES=src/gui.cpp
set CFLAGS=-std=c99 -O3 -Wall -Wextra -Werror -Wno-unused-parameter -Wformat-security -Wno-unused-result -pedantic -fPIC -DENABLE_GUI
set CXXFLAGS=-std=c++11 -O3 -Wall -Wextra -Werror -Wno-unused-parameter -Wformat-security -Wno-unused-result -pedantic -fPIC
set SDL_LINKS=-Lwinlib/SDL2/lib -lmingw32 -lSDL2main -lSDL2 -Lwinlib/SDL2_ttf/lib -lSDL2_ttf
set CURL_LINKS=-Lwinlib/curl/lib -lcurl
set INCLUDES=-Iinclude -Iwinlib/SDL2/include -Iwinlib/curl/include

g++ %INCLUDES% %CXXFLAGS% %CXX_FILES% -c -o winlib/gui.o
gcc -v %INCLUDES% %CFLAGS% %C_FILES% winlib/gui.o %SDL_LINKS% %CURL_LINKS% -o tekstitv.exe
