#ifndef _DRAWER_H_
#define _DRAWER_H_

#include <ncurses.h>
#include <stdbool.h>

typedef struct {
    html_link link;
    int start_x;
    int start_y;
} link_highlight;

typedef struct {
    link_highlight links[16];
    int size;
} link_highlight_row;

typedef struct {
    WINDOW* info_window;
    WINDOW* window;
    int window_start_x;
    int window_start_y;
    int w_width;
    int w_height;
    int current_x;
    int current_y;
    bool color_support;
    short text_color;
    short link_color;
    short background_color;

    /*
    Variables for navigation
    */
    int highlight_row;
    int highlight_col;
    bool init_highlight_rows;
    link_highlight_row highlight_rows[32];
    int highlight_row_size;
    bool error_drawn; // Was the last page drawn a load error page

} drawer;

void init_drawer(drawer* drawer);
void free_drawer(drawer* drawer);

void draw_parser();

#endif
