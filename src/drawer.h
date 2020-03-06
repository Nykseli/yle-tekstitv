#ifndef _DRAWER_H_
#define _DRAWER_H_

#include "html_parser.h"
#include <ncurses.h>
#include <stdbool.h>

typedef struct
{
    WINDOW* window;
    int w_width;
    int w_height;
    int current_x;
    int current_y;
    bool color_support;
    short text_color;
    short link_color;
    short background_color;
} drawer;

void init_drawer(drawer* drawer);
void free_drawer(drawer* drawer);

void draw_parser();

#endif
