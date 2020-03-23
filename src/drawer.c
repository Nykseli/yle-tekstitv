#include <locale.h>
#include <string.h>

#include "config.h"
#include "drawer.h"
#include "html_parser.h"
#include "page_loader.h"
#include "page_number.h"

#define MAX_MIDDLE_WIDTH (max_window_width() / 2)
#define MIDDLE_STARTX (MAX_MIDDLE_WIDTH / 2)
#define centerx(str_len) ((max_window_width() - (str_len)) / 2)

#define TEXT_COLOR_ID 1
#define TEXT_COLOR COLOR_PAIR(TEXT_COLOR_ID)
#define LINK_COLOR_ID 2
#define LINK_COLOR COLOR_PAIR(LINK_COLOR_ID)

typedef struct {
    char prev_page[HTML_LINK_SIZE];
    char next_page[HTML_LINK_SIZE];
    char prev_sub_page[HTML_LINK_SIZE];
    char next_sub_page[HTML_LINK_SIZE];
} navigation;

typedef enum {
    PREV_PAGE = 0,
    NEXT_PAGE,
    PREV_SUB_PAGE,
    NEXT_SUB_PAGE
} nav_type;

static navigation nav_links;

static void search_mode(drawer* drawer, html_parser* parser);

static int max_window_width()
{
    if (COLS > 80) {
        return 80;
    }

    return COLS;
}

static int middle_startx()
{
    if (COLS > 80) {
        return MIDDLE_STARTX;
    }

    return COLS / 5;
}

static void curl_load_error(drawer* drawer, html_parser* parser)
{
    clear();
    printw("Couldn't load the page, press any key to continue\n");
    refresh();
    getch();
    search_mode(drawer, parser);
}

static void escape_text(char* src, char* target, size_t src_len)
{
    size_t srci = 0;
    size_t targeti = 0;
    for (; srci < src_len; srci++, targeti++) {
        if (src[srci] == '%') {
            target[targeti] = '%';
            target[++targeti] = '%';
        } else {
            target[targeti] = src[srci];
        }
    }

    target[targeti] = '\0';
}

static void draw_to_drawer(drawer* drawer, char* text)
{
    char escape_buf[256];
    escape_text(text, escape_buf, strlen(text));
    mvwprintw(drawer->window, drawer->current_y, drawer->current_x, escape_buf);
}

static void draw_link_item(drawer* drawer, html_link link)
{
    bool highlight = false;
    if (!drawer->init_highlight_rows) {
        link_highlight hlight = drawer->highlight_rows[drawer->highlight_row].links[drawer->highlight_col];
        highlight = hlight.start_x == drawer->current_x && hlight.start_y == drawer->current_y;
    }

    if (drawer->color_support) {
        wattron(drawer->window, LINK_COLOR);
        if (highlight)
            wattron(drawer->window, A_REVERSE);
    }

    draw_to_drawer(drawer, html_link_text(link));

    if (drawer->color_support) {
        wattroff(drawer->window, LINK_COLOR);
        if (highlight)
            wattroff(drawer->window, A_REVERSE);
    }
}

static void add_link_highlight(drawer* drawer, html_link link)
{
    if (!drawer->init_highlight_rows)
        return;

    link_highlight h;
    strncpy(h.link, link.url.text, HTML_LINK_SIZE);
    h.start_x = drawer->current_x;
    h.start_y = drawer->current_y;
    link_highlight_row* row = &drawer->highlight_rows[drawer->highlight_row_size];
    row->links[row->size] = h;
    row->size++;
}

static void draw_title(drawer* drawer, html_parser* parser)
{
    if (global_config.no_title)
        return;

    drawer->current_x = centerx(parser->title.size);
    drawer->current_y++;
    draw_to_drawer(drawer, parser->title.text);
}

static void draw_top_navigation(drawer* drawer, html_parser* parser)
{
    if (global_config.no_nav || global_config.no_top_nav)
        return;

    html_item* items = parser->top_navigation;

    // Always collect navigation links for hotkeys
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        if (items[i].type != HTML_LINK)
            continue;

        switch (i) {
        case 0:
            strncpy(nav_links.prev_page, html_link_link(html_item_as_link(items[i])), HTML_LINK_SIZE);
            break;
        case 1:
            strncpy(nav_links.prev_sub_page, html_link_link(html_item_as_link(items[i])), HTML_LINK_SIZE);
            break;
        case 2:
            strncpy(nav_links.next_sub_page, html_link_link(html_item_as_link(items[i])), HTML_LINK_SIZE);
            break;
        case 3:
            strncpy(nav_links.next_page, html_link_link(html_item_as_link(items[i])), HTML_LINK_SIZE);
            break;
        default:
            break;
        }

        // strncpy(nav_links.prev_page, html_link_link(links[0]), HTML_LINK_SIZE);
        // strncpy(nav_links.prev_sub_page, html_link_link(links[1]), HTML_LINK_SIZE);
        // strncpy(nav_links.next_sub_page, html_link_link(links[2]), HTML_LINK_SIZE);
        // strncpy(nav_links.next_page, html_link_link(links[3]), HTML_LINK_SIZE);
    }

    // Only draw navigation on "big" terminals
    if (max_window_width() < 80)
        return;

    // Start length with adding sizes of " | " separators
    size_t links_len = (TOP_NAVIGATION_SIZE - 1) * 3;
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        links_len += html_item_text_size(items[i]);
    }

    // Draw navigation
    drawer->current_y += 2;
    drawer->current_x = (int)centerx(links_len);
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        if (items[i].type == HTML_LINK) {
            draw_link_item(drawer, html_item_as_link(items[i]));
            add_link_highlight(drawer, html_item_as_link(items[i]));
        } else {
            draw_to_drawer(drawer, html_text_text(html_item_as_text(items[i])));
        }

        drawer->current_x += html_item_text_size(items[i]);
        if (i < TOP_NAVIGATION_SIZE - 1) {
            draw_to_drawer(drawer, " |");
            drawer->current_x += 3;
        }
    }

    drawer->current_y += 1;
    if (drawer->init_highlight_rows) {
        drawer->highlight_row_size++;
    }
}

static void draw_bottom_navigation(drawer* drawer, html_parser* parser)
{
    if (global_config.no_nav || global_config.no_bottom_nav)
        return;

    // Only draw navigation on "big" terminals
    if (max_window_width() < 80)
        return;

    // first row is identical to top navigation
    draw_top_navigation(drawer, parser);

    html_link* links = parser->bottom_navigation;

    // int second_row_len = BOTTOM_NAVIGATION_SIZE - TOP_NAVIGATION_SIZE - 3;
    // int second_row_start = TOP_NAVIGATION_SIZE;
    // Start length with adding sizes of " | " separators
    size_t links_len = (BOTTOM_NAVIGATION_SIZE - 1) * 3;
    for (size_t i = 0; i < BOTTOM_NAVIGATION_SIZE; i++) {
        links_len += html_link_text_size(links[i]);
    }

    // Draw second navigation row
    drawer->current_y += 1;
    drawer->current_x = (int)centerx(links_len);
    for (size_t i = 0; i < BOTTOM_NAVIGATION_SIZE; i++) {
        draw_link_item(drawer, links[i]);
        add_link_highlight(drawer, links[i]);
        drawer->current_x += html_link_text_size(links[i]);
        if (i < BOTTOM_NAVIGATION_SIZE - 1) {
            draw_to_drawer(drawer, " |");
            drawer->current_x += 3;
        }
    }

    if (drawer->init_highlight_rows)
        drawer->highlight_row_size++;
}

static void draw_middle(drawer* drawer, html_parser* parser)
{
    if (global_config.no_middle)
        return;

    html_item_type last_type = HTML_TEXT;

    for (size_t i = 0; i < parser->middle_rows; i++) {
        drawer->current_x = middle_startx();
        drawer->current_y++;
        bool link_on_row = false;
        for (size_t j = 0; j < parser->middle[i].size; j++) {
            html_item item = parser->middle[i].items[j];
            if (item.type == HTML_LINK) {
                if (last_type == HTML_LINK) {
                    //drawer->current_x -= 1;
                    draw_to_drawer(drawer, "-");
                    drawer->current_x += 1;
                }
                draw_link_item(drawer, html_item_as_link(item));
                add_link_highlight(drawer, html_item_as_link(item));
                link_on_row = true;
                drawer->current_x += html_link_text_size(html_item_as_link(item));
                last_type = HTML_LINK;
            } else if (item.type == HTML_TEXT) {
                draw_to_drawer(drawer, html_text_text(html_item_as_text(item)));
                drawer->current_x += html_text_text_size(html_item_as_text(item));
                last_type = HTML_TEXT;
            }
        }

        if (drawer->init_highlight_rows && link_on_row)
            drawer->highlight_row_size++;
    }
}

static void redraw_parser(drawer* drawer, html_parser* parser, bool init)
{
    if (parser->curl_load_error) {
        curl_load_error(drawer, parser);
        return;
    }

    drawer->current_x = 0;
    drawer->current_y = 0;
    drawer->init_highlight_rows = init;
    if (init) {
        drawer->highlight_row = 0;
        drawer->highlight_col = 0;
        drawer->highlight_row_size = 0;
        memset(drawer->highlight_rows, 0, sizeof(link_highlight_row) * 32);
    }

    wclear(drawer->window);
    draw_title(drawer, parser);
    draw_top_navigation(drawer, parser);
    draw_middle(drawer, parser);
    draw_bottom_navigation(drawer, parser);
    wrefresh(drawer->window);
}

// Go round and round
static void next_col(drawer* drawer)
{
    link_highlight_row current = drawer->highlight_rows[drawer->highlight_row];
    drawer->highlight_col++;
    if (drawer->highlight_col >= current.size)
        drawer->highlight_col = 0;
}

static void prev_col(drawer* drawer)
{
    link_highlight_row current = drawer->highlight_rows[drawer->highlight_row];
    drawer->highlight_col--;
    if (drawer->highlight_col < 0)
        drawer->highlight_col = current.size - 1;
}

// Go round and round
static void next_row(drawer* drawer)
{
    drawer->highlight_row++;
    if (drawer->highlight_row >= drawer->highlight_row_size)
        drawer->highlight_row = 0;

    link_highlight_row next = drawer->highlight_rows[drawer->highlight_row];
    bool gotoend = drawer->highlight_col >= next.size;

    if (gotoend)
        drawer->highlight_col = next.size - 1;
}

static void prev_row(drawer* drawer)
{
    drawer->highlight_row--;
    if (drawer->highlight_row < 0)
        drawer->highlight_row = drawer->highlight_row_size - 1;

    link_highlight_row next = drawer->highlight_rows[drawer->highlight_row];
    bool gotoend = drawer->highlight_col >= next.size;

    if (gotoend)
        drawer->highlight_col = next.size - 1;
}

static void load_link(drawer* drawer, html_parser* parser, char* link, bool short_link)
{
    if (short_link)
        replace_link_part(link);

    free_html_parser(parser);
    init_html_parser(parser);

    load_page(parser, https_page_url);
    parse_html(parser);
    redraw_parser(drawer, parser, true);
}

static void load_highlight_link(drawer* drawer, html_parser* parser)
{
    link_highlight link = drawer->highlight_rows[drawer->highlight_row].links[drawer->highlight_col];
    load_link(drawer, parser, link.link, true);
}

static void load_nav_link(drawer* drawer, html_parser* parser, nav_type type)
{
    char* link = NULL;
    switch (type) {
    case NEXT_PAGE:
        link = nav_links.next_page;
        break;
    case PREV_PAGE:
        link = nav_links.prev_page;
        break;
    case NEXT_SUB_PAGE:
        link = nav_links.next_sub_page;
        break;
    case PREV_SUB_PAGE:
        link = nav_links.prev_sub_page;
        break;
    default:
        break;
    }

    load_link(drawer, parser, link, true);
}

void search_mode(drawer* drawer, html_parser* parser)
{

    mvprintw(0, 0, "                                                        ");
    mvprintw(0, 0, "Search page:");
    int currentx = 13;
    int page_i = 0;
    char page[4] = { 0, 0, 0, 0 };

    // Show cursor and inputs
    curs_set(2);
    refresh();
    while (true) {
        int c = getch();
        if (c == '\n') {
            int num = page_number(page);
            if (num == -1) {

                mvprintw(0, 0, "                                            ");
                mvprintw(0, 0, "Page needs to bee value between 100 and 999");
                refresh();
                getch();
                break;
            } else {
                add_page(num);
                add_subpage(1);
                load_link(drawer, parser, NULL, false);
                break;
            }
        } else if (c == KEY_BACKSPACE) {
            if (page_i == 0)
                continue;
            page_i--;
            currentx--;
            mvaddch(0, currentx, ' ');
            page[page_i] = 0;
        } else if (c == 27) { //esc
            break;
        } else {
            if (page_i + 1 >= 4)
                continue;
            mvaddch(0, currentx, (char)c);
            page[page_i] = (char)c;
            page_i++;
            currentx++;
        }
        refresh();
    }

    // hide cursor and inputs
    curs_set(0);
    mvprintw(0, 0, "                                            ");
    mvprintw(0, 0, "Press q to exit, s to search");
    refresh();
}

void main_draw_loop(drawer* drawer, html_parser* parser)
{
    drawer->highlight_col = -1;
    drawer->highlight_row = -1;
    while (true) {
        int c = getch();
        if (c == 'q') {
            break;
        } else if (c == 'h' || c == KEY_LEFT) {
            if (drawer->highlight_row == -1)
                drawer->highlight_row = 0;
            prev_col(drawer);
            redraw_parser(drawer, parser, false);
        } else if (c == 'k' || c == KEY_UP) {
            if (drawer->highlight_col == -1)
                drawer->highlight_col = 0;
            prev_row(drawer);
            redraw_parser(drawer, parser, false);
        } else if (c == 'j' || c == KEY_DOWN) {
            if (drawer->highlight_col == -1)
                drawer->highlight_col = 0;
            next_row(drawer);
            redraw_parser(drawer, parser, false);
        } else if (c == 'l' || c == KEY_RIGHT) {
            if (drawer->highlight_row == -1)
                drawer->highlight_row = 0;
            next_col(drawer);
            redraw_parser(drawer, parser, false);
        } else if (c == 'g' || c == '\n') { // g or enter
            load_highlight_link(drawer, parser);
        } else if (c == 'v') {
            load_nav_link(drawer, parser, PREV_PAGE);
        } else if (c == 'b') {
            load_nav_link(drawer, parser, PREV_SUB_PAGE);
        } else if (c == 'n') {
            load_nav_link(drawer, parser, NEXT_SUB_PAGE);
        } else if (c == 'm') {
            load_nav_link(drawer, parser, NEXT_PAGE);
        } else if (c == 's') {
            search_mode(drawer, parser);
        }
    }
}

void draw_parser(drawer* drawer, html_parser* parser)
{
    if (parser->curl_load_error) {
        curl_load_error(drawer, parser);
    } else {
        drawer->init_highlight_rows = true;
        memset(drawer->highlight_rows, 0, sizeof(link_highlight_row) * 32);

        draw_title(drawer, parser);
        draw_top_navigation(drawer, parser);
        draw_middle(drawer, parser);
        draw_bottom_navigation(drawer, parser);
        wrefresh(drawer->window);
    }

    main_draw_loop(drawer, parser);
    endwin();
}

void init_drawer(drawer* drawer)
{
    // Makes ncurses a utf-8 if host locale is utf-8
    setlocale(LC_ALL, "");
    // Start curses mode
    initscr();
    // Line buffering disabled, Pass on everty thing to me
    cbreak();
    // Don't show keypresses
    noecho();
    // All those F and arrow keys
    keypad(stdscr, TRUE);
    printw("Press q to exit, s to search");
    // Hide cursor
    curs_set(0);
    int window_start_x = (COLS - max_window_width()) / 2;
    int window_start_y = 1;

    drawer->w_width = max_window_width();
    drawer->w_height = LINES - window_start_y;
    drawer->current_x = 0;
    drawer->current_y = 0;
    drawer->color_support = has_colors();
    drawer->text_color = COLOR_WHITE;
    drawer->link_color = COLOR_BLUE;
    drawer->background_color = COLOR_BLACK;
    drawer->highlight_row = 0;
    drawer->highlight_col = 0;
    drawer->highlight_row_size = 0;
    drawer->window = newwin(drawer->w_height, drawer->w_width, window_start_y, window_start_x);

    if (drawer->color_support) {
        start_color();
        init_pair(TEXT_COLOR_ID, drawer->text_color, drawer->background_color);
        init_pair(LINK_COLOR_ID, drawer->link_color, drawer->background_color);
        bkgd(TEXT_COLOR);
        wbkgd(drawer->window, TEXT_COLOR);
    }

    refresh();
}

void free_drawer(drawer* drawer)
{
    delwin(drawer->window);
}
