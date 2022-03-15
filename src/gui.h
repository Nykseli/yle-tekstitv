#ifndef _GUI_H_
#define _GUI_H_

// gui needs to be in c++ since we're going to use imgui that requires c++
#ifdef __cplusplus
extern "C" {
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <tekstitv.h>

/**
 * gui_text contains data for each visible text object on the window
 */
typedef struct gui_text {
    SDL_Rect rect;
    SDL_Texture* texture;
} gui_text;

typedef struct gui_drawer {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    int line_height; // height of a single character
    int char_width; // width of a single character

    gui_text* texts;
    int text_count;

    int w_width;
    int w_height;
    int current_x;
    int current_y;
} gui_drawer;

int display_gui(html_parser* parser);

#ifdef __cplusplus
}
#endif

#endif
