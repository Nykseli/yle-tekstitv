# Windows info

## How to install MinGW

https://www.youtube.com/watch?v=WWTocqPrzMk

## How to install libcurl

Download 32 bit curl from
https://curl.se/windows/

Then extract the content to projects root, under `winlib/curl` folder.

If you're using vscode, add `"winlib/curl/include"` to  `"C_Cpp.default.includePath"` in settings so vscode can find the header file.

You also need to download `cacert.pem` from https://curl.se/docs/caextract.html and add it to the project root

## How to install SDL

Download the MinGW development files from
https://www.libsdl.org/download-2.0.php

Then extact the content from `i686-w64-mingw32` to `winlib/SDL2` folder.


If you're using vscode, add `"winlib/SDL2/include"` to  `"C_Cpp.default.includePath"` in settings so vscode can find the header file.

## How to install SDL_ttf

Download the MinGW development files from
https://github.com/libsdl-org/SDL_ttf/releases/tag/release-2.0.18

Then extact the content from `i686-w64-mingw32` to `winlib/SDL2_ttf` folder.
Then copy the `winlib/SDL2_ttf/include/SDL2/SDL_ttf.h` file to `winlib/SDL2/include/SDL2/SDL_ttf.h`
