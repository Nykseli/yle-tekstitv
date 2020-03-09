#include <string.h>

#include "drawer.h"
#include "html_parser.h"
#include "page_loader.h"

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

static void draw_to_drawer(drawer* drawer, char* text)
{
    mvwprintw(drawer->window, drawer->current_y, drawer->current_x, text);
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
    drawer->current_x = centerx(parser->title.size);
    drawer->current_y++;
    draw_to_drawer(drawer, parser->title.text);
}

static void draw_top_navigation(drawer* drawer, html_parser* parser)
{
    html_link* links = parser->top_navigation;

    // Always collect navigation links for hotkeys
    strncpy(nav_links.prev_page, html_link_link(links[0]), HTML_LINK_SIZE);
    strncpy(nav_links.prev_sub_page, html_link_link(links[1]), HTML_LINK_SIZE);
    strncpy(nav_links.next_sub_page, html_link_link(links[2]), HTML_LINK_SIZE);
    strncpy(nav_links.next_page, html_link_link(links[3]), HTML_LINK_SIZE);

    // Only draw navigation on "big" terminals
    if (max_window_width() < 80)
        return;

    // Start length with adding sizes of " | " separators
    size_t links_len = (TOP_NAVIGATION_SIZE - 1) * 3;
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        links_len += links[i].inner_text.size;
    }

    // Draw navigation
    drawer->current_y += 2;
    drawer->current_x = (int)centerx(links_len);
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        draw_link_item(drawer, links[i]);
        add_link_highlight(drawer, links[i]);
        drawer->current_x += html_link_text_size(links[i]);
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
    // Only draw navigation on "big" terminals
    if (max_window_width() < 80)
        return;

    // first row is identical to top navigation
    draw_top_navigation(drawer, parser);

    html_row links = parser->bottom_navigation[1];

    // int second_row_len = BOTTOM_NAVIGATION_SIZE - TOP_NAVIGATION_SIZE - 3;
    // int second_row_start = TOP_NAVIGATION_SIZE;
    // Start length with adding sizes of " | " separators
    size_t links_len = (links.size - 1) * 3;
    for (size_t i = 0; i < links.size; i++) {
        links_len += html_link_text_size(html_item_as_link(links.items[i]));
    }

    // Draw second navigation row
    drawer->current_y += 1;
    drawer->current_x = (int)centerx(links_len);
    for (size_t i = 0; i < links.size; i++) {
        draw_link_item(drawer, html_item_as_link(links.items[i]));
        add_link_highlight(drawer, html_item_as_link(links.items[i]));
        drawer->current_x += html_link_text_size(html_item_as_link(links.items[i]));
        if (i < links.size - 1) {
            draw_to_drawer(drawer, " |");
            drawer->current_x += 3;
        }
    }

    if (drawer->init_highlight_rows)
        drawer->highlight_row_size++;
}

static void draw_middle(drawer* drawer, html_parser* parser)
{
    html_item_type last_type = HTML_TEXT;

    for (size_t i = 0; i < parser->middle_rows; i++) {
        drawer->current_x = middle_startx();
        drawer->current_y++;
        bool link_on_row = false;
        for (size_t j = 0; j < parser->middle[i].size; j++) {
            html_item item = parser->middle[i].items[j];
            if (item.type == HTML_LINK) {
                if (last_type == HTML_LINK) {
                    drawer->current_x -= 1;
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

static void load_link(drawer* drawer, html_parser* parser, char* link)
{
    // Link + https: + null
    char https_link[HTML_LINK_SIZE + 7] = { 'h', 't', 't', 'p', 's', ':' };
    strncpy(https_link + 6, link, HTML_LINK_SIZE);
    https_link[HTML_LINK_SIZE + 6] = '\0';

    free_html_parser(parser);
    init_html_parser(parser);

    load_page(parser, https_link);
    parse_html(parser);
    redraw_parser(drawer, parser, true);
}

static void load_highlight_link(drawer* drawer, html_parser* parser)
{
    link_highlight link = drawer->highlight_rows[drawer->highlight_row].links[drawer->highlight_col];
    load_link(drawer, parser, link.link);
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

    load_link(drawer, parser, link);
}

void draw_parser(drawer* drawer, html_parser* parser)
{
    drawer->init_highlight_rows = true;
    memset(drawer->highlight_rows, 0, sizeof(link_highlight_row) * 32);

    draw_title(drawer, parser);
    draw_top_navigation(drawer, parser);
    draw_middle(drawer, parser);
    draw_bottom_navigation(drawer, parser);
    wrefresh(drawer->window);
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
        }
    }
    endwin();
}

void init_drawer(drawer* drawer)
{
    // Start curses mode
    initscr();
    // Line buffering disabled, Pass on everty thing to me
    cbreak();
    // Don't show keypresses
    noecho();
    // All those F and arrow keys
    keypad(stdscr, TRUE);
    printw("Press q to exit");
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
