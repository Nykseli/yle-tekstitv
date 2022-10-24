#ifndef DISABLE_UTF8
#include <locale.h>
#include <stdlib.h>
#endif

#include <string.h>
#include <tekstitv.h>

#include "config.h"
#include "drawer.h"

#define MAX_MIDDLE_WIDTH (max_window_width() / 2)
#define MIDDLE_STARTX (MAX_MIDDLE_WIDTH / 2)
#define centerx(str_len) ((max_window_width() - (str_len)) / 2)

#define TEXT_COLOR_ID 1
#define TEXT_COLOR COLOR_PAIR(TEXT_COLOR_ID)
#define LINK_COLOR_ID 2
#define LINK_COLOR COLOR_PAIR(LINK_COLOR_ID)

#define HISTORY_CAPACITY 16

#define HISTORY_NEXT(val) (((val) + 1) % HISTORY_CAPACITY)
#define HISTORY_PREV(val) (((val) + HISTORY_CAPACITY - 1) % HISTORY_CAPACITY)
#define HISTORY_CURRENT_LINK (history.entries[history.current])

typedef struct {
    // Storage for browser links
    char entries[HISTORY_CAPACITY][HTML_LINK_SIZE];
    int count; // Amount of items in the buffer
    int start; // Start of the valid data
    int current; // Index of the value we are currently using
} browser_history;

browser_history history = {
    .count = 0,
    .start = 0,
    .current = -1,
};

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
static link_highlight* old_link = NULL;

static void search_mode(drawer* drawer, html_parser* parser);
static void load_link(drawer* drawer, html_parser* parser, bool add_history);
static void draw_to_info_window(drawer* drawer, const char* text);
static int handle_getch(drawer* drawer, html_parser* parser);
static void redraw_parser(drawer* drawer, html_parser* parser, bool init, bool add_history);

#ifdef DEBUG
/**
 * Append text to /tmp/tekstitv.debug.log
 */
#include <stdarg.h>
static void draw_logger(const char* format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    FILE* tmpFile = fopen("/tmp/tekstitv.debug.log", "a");
    vfprintf(tmpFile, format, argptr);
    va_end(argptr);
    fclose(tmpFile);
}
#endif // DEBUG_LOGGER

static bool history_at_last_link()
{
    if (history.start == 0) {
        if (history.current == history.count - 1)
            return true;
    } else if (history.current == history.start - 1) {
        return true;
    }

    return false;
}

static void add_history_link(char* link)
{
    bool at_last = history_at_last_link();

    // If history is not full and we are at the last link we can just add new link
    if (history.count != HISTORY_CAPACITY && at_last) {
        history.count++;
        history.current = HISTORY_NEXT(history.current);
        memcpy(history.entries[history.current], link, HTML_LINK_SIZE);
        return;
    }

    // If not at the last link, "Override" the links after current
    if (!at_last) {
        // + 2 Because there will be atleast the current with the upcoming link
        if (history.start == 0 || history.start < history.current) {
            history.count = history.current - history.start + 2;
        } else {
            history.count = HISTORY_CAPACITY - (history.start - history.current) + 2;
        }
    } else {
        history.start = HISTORY_NEXT(history.start);
    }

    history.current = HISTORY_NEXT(history.current);
    memcpy(history.entries[history.current], link, HTML_LINK_SIZE);
}

static char* next_link()
{
    if (history.count == 0)
        return NULL;

    if (history_at_last_link())
        return NULL;

    history.current = HISTORY_NEXT(history.current);
    return HISTORY_CURRENT_LINK;
}

static char* prev_link()
{
    if (history.count == 0 || history.current == history.start)
        return NULL;

    history.current = HISTORY_PREV(history.current);
    return HISTORY_CURRENT_LINK;
}

static void load_next_link(drawer* drawer, html_parser* parser)
{
    char* link = next_link();
    // TODO: let user know that there is no next link
    if (link != NULL) {
        link_from_short_link(parser, link);
        load_link(drawer, parser, false);
    }
}

static void load_prev_link(drawer* drawer, html_parser* parser)
{
    char* link = prev_link();
    // TODO: let user know that there is no previous link
    if (link != NULL) {
        link_from_short_link(parser, link);
        load_link(drawer, parser, false);
    }
}

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
    // Kind of a hack to get history working with load errors
    if (!drawer->error_drawn) {
        history.current = HISTORY_NEXT(history.current);
        drawer->error_drawn = true;
    }

    draw_to_info_window(drawer, "Couldn't load the page, press s to search, o to return");
    for (;;) {
        char c = handle_getch(drawer, parser);
        if (c == 's') {
            search_mode(drawer, parser);
            break;
        } else if (c == 'o') {
            load_prev_link(drawer, parser);
            break;
        }
    }
}

/**
 * Set/reset the main window
 */
static void set_main_window_size(drawer* drawer)
{
    if (drawer->window != NULL)
        delwin(drawer->window);
    if (drawer->info_window == NULL)
        drawer->info_window = newwin(1, max_window_width(), 0, 0);

    drawer->window_start_x = (COLS - max_window_width()) / 2;
    drawer->window_start_y = 1;

    drawer->w_width = max_window_width();
    drawer->w_height = LINES - drawer->window_start_y;
    drawer->current_x = 0;
    drawer->current_y = 0;
    drawer->color_support = has_colors();
    drawer->text_color = -1;
    drawer->link_color = -1;
    drawer->background_color = -1;
    drawer->highlight_row = -1;
    drawer->highlight_col = -1;
    drawer->highlight_row_size = 0;
    drawer->error_drawn = false;
    drawer->window = newwin(drawer->w_height, drawer->w_width, drawer->window_start_y, drawer->window_start_x);

    if (drawer->color_support && !global_config.default_colors) {
        use_default_colors();
        start_color();
        // Link color is by default the default blue
        // Other colors are defined by the console theme by default
        drawer->link_color = COLOR_BLUE;
        // Redefine the colors if user has set them
        if (can_change_color()) {
            if (BG_RGB(0) != -1) {
                drawer->background_color = COLOR_BLACK;
                init_color(COLOR_BLACK, BG_RGB(0), BG_RGB(1), BG_RGB(2));
            }
            if (LINK_RGB(0) != -1)
                init_color(COLOR_BLUE, LINK_RGB(0), LINK_RGB(1), LINK_RGB(2));
            if (TEXT_RGB(0) != -1) {
                drawer->text_color = COLOR_WHITE;
                init_color(COLOR_WHITE, TEXT_RGB(0), TEXT_RGB(1), TEXT_RGB(2));
            }
        }
        init_pair(TEXT_COLOR_ID, drawer->text_color, drawer->background_color);
        init_pair(LINK_COLOR_ID, drawer->link_color, drawer->background_color);
        bkgd(TEXT_COLOR);
        wbkgd(drawer->window, TEXT_COLOR);
        wbkgd(drawer->info_window, TEXT_COLOR);
    }

    refresh();
}

static bool find_link_highlight(drawer* drawer, int x, int y, char* link_target)
{
    int hrows = drawer->highlight_row_size;
    link_highlight_row* lrows = drawer->highlight_rows;
    for (int i = 0; i < hrows; i++) {
        for (int j = 0; j < lrows[i].size; j++) {
            link_highlight link = lrows[i].links[j];
            bool match_y = y - drawer->window_start_y == link.start_y;
            int adjustx = x - drawer->window_start_x;
            if (match_y && adjustx >= link.start_x && adjustx <= link.start_x + (int)strlen(html_link_text(link.link))) {
                strcpy(link_target, html_link_link(link.link));
                return true;
            }
        }
    }
    return false;
}

/**
 * Handle the ncurses getch function and return its value
 * after handling the required functionality
 */
static int handle_getch(drawer* drawer, html_parser* parser)
{
    int c = getch();
    if (c == KEY_RESIZE) {
        set_main_window_size(drawer);
        redraw_parser(drawer, parser, true, false);
    }

    return c;
}

/**
 * Find how many uft characters are found in the text.
 *
 * Utf-8 characters used in tekstitv are always 2 bytes but are displayed
 * as one character. This cofuses the drawer->curret_x counter since it
 * counts every byte as a displayed character.
 * Workaround for this is just to find all the utfs used and remove the extra
 * spaces from drawer->current_x.
 *
 * // TODO: combine this with escape_text or find otherwise smarter way to do this
 */
static int find_utfs(const char* src, size_t src_len)
{
    int utfs = 0;
    for (size_t i = 0; i < src_len; i++) {
        if ((uint8_t)src[i] == 0xc3 || (uint8_t)src[i] == 0xc2)
            utfs++;
    }
    return utfs;
}

static void escape_text(const char* src, char* target, size_t src_len)
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

static void draw_to_drawer(drawer* drawer, const char* text)
{
    char escape_buf[256];
    int utfs = find_utfs(text, strlen(text));
    escape_text(text, escape_buf, strlen(text));
    mvwprintw(drawer->window, drawer->current_y, drawer->current_x, "%s", escape_buf);
    drawer->current_x -= utfs;
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
    strncpy(html_link_link(h.link), link.url.text, HTML_LINK_SIZE);
    html_link_link(h.link)[HTML_LINK_SIZE] = '\0';
    strncpy(html_link_text(h.link), link.inner_text.text, link.inner_text.size);
    html_link_text(h.link)[link.inner_text.size] = '\0';
    h.start_x = drawer->current_x;
    h.start_y = drawer->current_y;
    link_highlight_row* row = &drawer->highlight_rows[drawer->highlight_row_size];
    row->links[row->size] = h;
    row->size++;
}

/**
 * Draw on the first line of the drawer. This should only contain things
 * like tips and the page search.
 */
static void draw_to_info_window(drawer* drawer, const char* text)
{
    wclear(drawer->info_window);
    mvwprintw(drawer->info_window, 0, 0, "%s", text);
    wrefresh(drawer->info_window);
}

/**
 * Draw title and current time
 */
static void draw_title(drawer* drawer, html_parser* parser)
{
    bool draw_title = !global_config.no_title;
    bool draw_time = global_config.time_fmt != NULL;

    if (!draw_title && !draw_time)
        return;

    fmt_time ctime = current_time();

    size_t text_len = 0;
    if (draw_title)
        text_len += parser->title.size;

    if (draw_time)
        text_len += ctime.time_len;

    drawer->current_x = centerx(text_len);
    drawer->current_y++;

    if (draw_title) {
        draw_to_drawer(drawer, parser->title.text);
        drawer->current_x += parser->title.size + 1;
    }

    if (draw_time)
        draw_to_drawer(drawer, ctime.time);
}

static void draw_top_navigation(drawer* drawer, html_parser* parser)
{
    html_item* items = parser->top_navigation;

    // Always collect navigation links for hotkeys
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        // If the item doesn't contain an actual link,
        // make sure the navigation link is null
        switch (i) {
        case 0:
            if (items[i].type == HTML_LINK)
                strncpy(nav_links.prev_page, html_link_link(html_item_as_link(items[i])), HTML_LINK_SIZE);
            else
                nav_links.prev_page[0] = 0;
            break;
        case 1:
            if (items[i].type == HTML_LINK)
                strncpy(nav_links.prev_sub_page, html_link_link(html_item_as_link(items[i])), HTML_LINK_SIZE);
            else
                nav_links.prev_sub_page[0] = 0;
            break;
        case 2:
            if (items[i].type == HTML_LINK)
                strncpy(nav_links.next_sub_page, html_link_link(html_item_as_link(items[i])), HTML_LINK_SIZE);
            else
                nav_links.next_sub_page[0] = 0;
            break;
        case 3:
            if (items[i].type == HTML_LINK)
                strncpy(nav_links.next_page, html_link_link(html_item_as_link(items[i])), HTML_LINK_SIZE);
            else
                nav_links.next_page[0] = 0;
            break;
        default:
            break;
        }
    }

    if (global_config.no_nav || global_config.no_top_nav)
        return;

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

static void draw_sub_pages(drawer* drawer, html_parser* parser)
{
    if (global_config.no_sub_page)
        return;

    drawer->current_x = middle_startx();
    drawer->current_y++;
    bool link_on_row = false;
    for (size_t i = 0; i < parser->sub_pages.size; i++) {
        html_item item = parser->sub_pages.items[i];
        if (item.type == HTML_LINK) {
            draw_link_item(drawer, html_item_as_link(item));
            add_link_highlight(drawer, html_item_as_link(item));
            link_on_row = true;
            drawer->current_x += html_link_text_size(html_item_as_link(item));
        } else if (item.type == HTML_TEXT) {
            draw_to_drawer(drawer, html_text_text(html_item_as_text(item)));
            drawer->current_x += html_text_text_size(html_item_as_text(item));
        }
    }

    if (drawer->init_highlight_rows && link_on_row)
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
                    // drawer->current_x -= 1;
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

/**
 * Update link highlights without drawing the whole window
 */
static void update_link_highlights(drawer* drawer)
{
    // hightlights rows are initialized before this
    // so set this to false so highlighting works in draw_link_item
    drawer->init_highlight_rows = false;

    link_highlight* new_link = &drawer->highlight_rows[drawer->highlight_row].links[drawer->highlight_col];
    drawer->current_x = new_link->start_x;
    drawer->current_y = new_link->start_y;
    draw_link_item(drawer, new_link->link);

    if (old_link != NULL) {
        drawer->current_x = old_link->start_x;
        drawer->current_y = old_link->start_y;
        draw_link_item(drawer, old_link->link);
    }

    old_link = new_link;
    wrefresh(drawer->window);
}

static void redraw_parser(drawer* drawer, html_parser* parser, bool init, bool add_history)
{
    if (parser->curl_load_error) {
        curl_load_error(drawer, parser);
        return;
    }

    if (add_history)
        add_history_link(parser->link);

    drawer->current_x = 0;
    drawer->current_y = 0;
    drawer->error_drawn = false;
    drawer->init_highlight_rows = init;
    if (init) {
        // row and col needs to be -1 so it becomes + after +1
        drawer->highlight_row = -1;
        drawer->highlight_col = -1;
        drawer->highlight_row_size = 0;
        memset(drawer->highlight_rows, 0, sizeof(link_highlight_row) * 32);
    }

    wclear(drawer->window);
    draw_to_info_window(drawer, "Press q to exit, s to search");
    draw_title(drawer, parser);
    draw_top_navigation(drawer, parser);
    draw_middle(drawer, parser);
    draw_sub_pages(drawer, parser);
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

static void load_link(drawer* drawer, html_parser* parser, bool add_history)
{
    draw_to_info_window(drawer, "Loading page...");
    free_html_parser(parser);
    init_html_parser(parser);

    load_page(parser);
    parse_html(parser);
    redraw_parser(drawer, parser, true, add_history);
}

static void load_highlight_link(drawer* drawer, html_parser* parser)
{
    // Make sure that something has been highlighted
    if (drawer->highlight_col == -1 || drawer->highlight_row == -1)
        return;

    link_highlight link = drawer->highlight_rows[drawer->highlight_row].links[drawer->highlight_col];
    link_from_short_link(parser, html_link_link(link.link));
    load_link(drawer, parser, true);
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

    // Links first character is set to null in draw_top_navigation
    // if the there is no actual page in the current page
    if (link != NULL && link[0] != 0) {
        link_from_short_link(parser, link);
        load_link(drawer, parser, true);
    }
}

/**
 * Print line to screen and increment the line number.
 */
static void print_navigation_line(drawer* drawer, const char* line)
{
    mvwprintw(drawer->window, drawer->current_y, drawer->current_x, "%s", line);
    drawer->current_y++;
}

/**
 * Show instructions for navigating the program
 */
static void draw_navigation_screen(drawer* drawer, html_parser* parser)
{
    // First clear the window
    wclear(drawer->window);
    draw_to_info_window(drawer, "Press q to return");

    drawer->current_x = middle_startx();
    drawer->current_y = 2;
    print_navigation_line(drawer, "|   Key   |         Action         |");
    print_navigation_line(drawer, "|----------------------------------|");
    print_navigation_line(drawer, "| j/Down  | Move one link down     |");
    print_navigation_line(drawer, "| k/Up    | Move one link up       |");
    print_navigation_line(drawer, "| l/Right | Move one link right    |");
    print_navigation_line(drawer, "| h/Left  | Move one link left     |");
    print_navigation_line(drawer, "| g/Enter | Open the selected link |");
    print_navigation_line(drawer, "| v       | Load previous page     |");
    print_navigation_line(drawer, "| m       | Load next page         |");
    print_navigation_line(drawer, "| b       | Load previous sub page |");
    print_navigation_line(drawer, "| n       | Load next sub page     |");
    print_navigation_line(drawer, "| s       | Search new page        |");
    print_navigation_line(drawer, "| r       | Reload page            |");
    print_navigation_line(drawer, "| o       | Previous page          |");
    print_navigation_line(drawer, "| p       | Next page              |");
    print_navigation_line(drawer, "| i       | Show navigation help   |");
    print_navigation_line(drawer, "| esc     | Cancel search mode     |");
    print_navigation_line(drawer, "| q       | Quit program           |");

    // Refresh to show the new text
    wrefresh(drawer->window);

    // Just wait until q is pressed so we can redraw
    while (true) {
        int c = handle_getch(drawer, parser);
        if (c == KEY_RESIZE) {
            // return on KEY_RESIZE
            // handle_getch redraws the parser for us
            return;
        } else if (c == 'q') {
            break;
        }
    }

    redraw_parser(drawer, parser, false, false);
}

void search_mode(drawer* drawer, html_parser* parser)
{

    draw_to_info_window(drawer, "Search page:");
    int currentx = 13;
    int page_i = 0;
    char page[4] = { 0, 0, 0, 0 };

    while (true) {
        int c = handle_getch(drawer, parser);
        if (c == KEY_BACKSPACE) {
            if (page_i == 0)
                continue;
            page_i--;
            currentx--;
            mvaddch(0, currentx, ' ');
            page[page_i] = 0;
        } else if (c == 27) { // esc
            break;
        } else {
            if (c < '0' || c > '9')
                continue;

            mvaddch(0, currentx, (char)c);
            page[page_i] = (char)c;
            page_i++;
            currentx++;
            refresh();

            if (page_i == 3) {
                int num = page_number(page);
                if (num == -1) {
                    draw_to_info_window(drawer, "Page needs to bee value between 100 and 999");
                    refresh();
                    handle_getch(drawer, parser);
                } else {
                    link_from_ints(parser, num, 1);
                    load_link(drawer, parser, true);
                }
                break;
            }
        }
    }

    draw_to_info_window(drawer, "Press q to exit, s to search");
}

void main_draw_loop(drawer* drawer, html_parser* parser)
{
    drawer->highlight_col = -1;
    drawer->highlight_row = -1;
    while (true) {
        int c = handle_getch(drawer, parser);
        if (c == 'q') {
            break;
        } else if (c == 'h' || c == KEY_LEFT) {
            if (drawer->highlight_row == -1)
                drawer->highlight_row = 0;
            prev_col(drawer);
            update_link_highlights(drawer);
        } else if (c == 'k' || c == KEY_UP) {
            if (drawer->highlight_col == -1)
                drawer->highlight_col = 0;
            prev_row(drawer);
            update_link_highlights(drawer);
        } else if (c == 'j' || c == KEY_DOWN) {
            if (drawer->highlight_col == -1)
                drawer->highlight_col = 0;
            next_row(drawer);
            update_link_highlights(drawer);
        } else if (c == 'l' || c == KEY_RIGHT) {
            if (drawer->highlight_row == -1)
                drawer->highlight_row = 0;
            next_col(drawer);
            update_link_highlights(drawer);
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
        } else if (c == 'i') {
            draw_navigation_screen(drawer, parser);
        } else if (c == 'o') {
            load_prev_link(drawer, parser);
        } else if (c == 'p') {
            load_next_link(drawer, parser);
        } else if (c == 's') {
            search_mode(drawer, parser);
        } else if (c == 'r') {
            load_link(drawer, parser, false);
        } else if (c == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK && event.bstate & BUTTON1_CLICKED) {
                char link_buf[HTML_LINK_SIZE + 1];
                if (find_link_highlight(drawer, event.x, event.y, link_buf)) {
                    link_from_short_link(parser, link_buf);
                    load_link(drawer, parser, true);
                }
            }
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

        draw_to_info_window(drawer, "Press q to exit, s to search");
        draw_title(drawer, parser);
        draw_top_navigation(drawer, parser);
        draw_middle(drawer, parser);
        draw_sub_pages(drawer, parser);
        draw_bottom_navigation(drawer, parser);
        wrefresh(drawer->window);

        add_history_link(parser->link);
    }

    main_draw_loop(drawer, parser);
    endwin();
}

void init_drawer(drawer* drawer)
{
#ifndef DISABLE_UTF_8
    // Make sure that locale is set so ncurses can show UTF-8 properly
    // Set the locale as the enviroment language
    // Language is usually in either LANG or LANGUAGE env variable
    char* lang = getenv("LANG");
    if (lang == NULL)
        lang = getenv("LANGUAGE");

    // Lang is either the env variable or an empty string
    if (lang == NULL)
        lang = "";

    setlocale(LC_ALL, lang);
#endif
    // Start curses mode
    initscr();
    // Line buffering disabled, Pass on everty thing to me
    cbreak();
    // Don't show keypresses
    noecho();
    // All those F and arrow keys
    keypad(stdscr, TRUE);
    // Hide cursor
    curs_set(0);
    // Get all the mouse events
    mousemask(ALL_MOUSE_EVENTS, NULL);

    drawer->window = NULL;
    drawer->info_window = NULL;
    set_main_window_size(drawer);
}

void free_drawer(drawer* drawer)
{
    delwin(drawer->window);
    delwin(drawer->info_window);
}
