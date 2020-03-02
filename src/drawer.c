#include "drawer.h"

#define MAX_WINDOW_WIDTH 80
#define MAX_MIDDLE_WIDTH (MAX_WINDOW_WIDTH / 2)
#define centerx(str_len) ((MAX_WINDOW_WIDTH - (str_len)) / 2)

static void draw_to_drawer(drawer* drawer, char* text)
{
    mvwprintw(drawer->window, drawer->current_y, drawer->current_x, text);
}

static void draw_title(drawer* drawer, html_parser* parser)
{
    drawer->current_x = centerx(parser->title.size);
    drawer->current_y++;
    draw_to_drawer(drawer, parser->title.text);
}

static void draw_top_navigation(drawer* drawer, html_parser* parser)
{
    html_link* links = parser->top_navigation;
    // Start length with adding sizes of " | " separators
    size_t links_len = (TOP_NAVIGATION_SIZE - 1) * 2;
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        links_len += links[i].inner_text.size;
    }

    // Draw navigation
    drawer->current_y += 2;
    drawer->current_x = (int)centerx(links_len);
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        draw_to_drawer(drawer, html_link_text(links[i]));
        drawer->current_x += html_link_text_size(links[i]);
        if (i < TOP_NAVIGATION_SIZE - 1) {
            draw_to_drawer(drawer, "|");
            drawer->current_x += 2;
        }
    }
}

static void draw_bottom_navigation(drawer* drawer, html_parser* parser)
{
    // first row is identical to top navigation
    draw_top_navigation(drawer, parser);

    html_link* links = parser->bottom_navigation;

    int second_row_len = BOTTOM_NAVIGATION_SIZE - TOP_NAVIGATION_SIZE - 3;
    int second_row_start = TOP_NAVIGATION_SIZE;
    // Start length with adding sizes of " | " separators
    int links_len = (second_row_len - 1) * 2;
    for (int i = second_row_start; i < second_row_start + second_row_len; i++) {
        links_len += links[i].inner_text.size;
    }

    // Draw second navigation row
    drawer->current_y += 1;
    drawer->current_x = (int)centerx(links_len);
    for (int i = second_row_start; i < second_row_start + second_row_len; i++) {
        draw_to_drawer(drawer, html_link_text(links[i]));
        drawer->current_x += html_link_text_size(links[i]);
        if (i < second_row_start + second_row_len - 1) {
            draw_to_drawer(drawer, "|");
            drawer->current_x += 2;
        }
    }
}

static size_t middle_row_length(drawer* drawer, html_parser* parser, size_t current, size_t* end)
{
    size_t current_row_length = 0;
    for (; current < parser->middle_size; current++) {
        size_t item_len = html_item_text_size(parser->middle[current]);
        current_row_length += item_len;
        if (item_len + current_row_length >= MAX_MIDDLE_WIDTH)
            break;
    }

    *end = current;
    return current_row_length;
}

static void draw_middle(drawer* drawer, html_parser* parser)
{
    html_item_type last_type = HTML_TEXT;
    size_t end_index = 0;
    size_t current_index = 0;
    size_t row_length = 0;

    while (current_index < parser->middle_size) {
        //printf("While loop\n");
        row_length = middle_row_length(drawer, parser, current_index, &end_index);
        drawer->current_x = centerx(row_length);
        drawer->current_y++;
        for (; current_index <= end_index; current_index++) {
            html_item item = parser->middle[current_index];
            if (item.type == HTML_LINK) {
                if (last_type == HTML_LINK) {
                    drawer->current_x -= 1;
                    draw_to_drawer(drawer, "-");
                    drawer->current_x += 1;
                }
                draw_to_drawer(drawer, html_link_text(html_item_as_link(item)));
                drawer->current_x += html_link_text_size(html_item_as_link(item));
                last_type = HTML_LINK;
            } else if (item.type == HTML_TEXT) {
                draw_to_drawer(drawer, html_text_text(html_item_as_text(item)));
                drawer->current_x += html_text_text_size(html_item_as_text(item));
                last_type = HTML_TEXT;
            }
        }
    }
}

void draw_parser(drawer* drawer, html_parser* parser)
{
    printw(" middle len: %ld ", parser->middle_size);
    draw_title(drawer, parser);
    draw_top_navigation(drawer, parser);
    draw_middle(drawer, parser);
    draw_bottom_navigation(drawer, parser);
    box(drawer->window, 0, 0);
    wrefresh(drawer->window);
    while (getch() != 'q')
        ;
    endwin();
}

void init_drawer(drawer* drawer)
{
    // Start curses mode
    initscr();
    // Line buffering disabled, Pass on everty thing to me
    cbreak();
    // All those F and arrow keys
    keypad(stdscr, TRUE);
    printw("Press q to exit");
    refresh();
    int window_start_x = (COLS - MAX_WINDOW_WIDTH) / 2;
    int window_start_y = 1;

    drawer->w_width = MAX_WINDOW_WIDTH;
    drawer->w_height = LINES - window_start_y;
    drawer->current_x = 0;
    drawer->current_y = 0;
    drawer->window = newwin(drawer->w_height, drawer->w_width, window_start_y, window_start_x);
}

void free_drawer(drawer* drawer)
{
    delwin(drawer->window);
}
