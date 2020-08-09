#ifndef _TEKSTITV_H_
#define _TEKSTITV_H_
#define _TEKSTITV

#define TEKSTITV_MAJOR_VERSION 0
#define TEKSTITV_MINOR_VERSION 5

#include <stdbool.h>
#include <stddef.h>

#define HTML_TEXT_MAX 64
// How many html_items can be on html_row
#define HTML_ROW_MAX 16
// How many html_rows the can be in the middle
#define MIDDLE_HTML_ROWS_MAX 32
#define TOP_NAVIGATION_SIZE 4
#define BOTTOM_NAVIGATION_SIZE 6
// All links parsed from html document are 12 characters
#define HTML_LINK_SIZE 12

typedef enum {
    HTML_TEXT,
    HTML_LINK,
} html_item_type;

typedef struct {
    char text[HTML_TEXT_MAX];
    size_t size;
} html_text;

typedef struct {
    html_text url;
    html_text inner_text;
} html_link;

typedef struct {
    html_item_type type;
    union {
        html_text text;
        html_link link;
    } item;
} html_item;

typedef struct {
    html_item items[HTML_ROW_MAX];
    size_t size;
} html_row;

typedef struct {
    char html[1024 * 32];
    size_t size;
    size_t current;
} html_buffer;

typedef struct {
    // title seems to always be 1 string
    html_text title;
    html_item top_navigation[TOP_NAVIGATION_SIZE];
    //html_link bottom_navigation[BOTTOM_NAVIGATION_SIZE];
    html_link bottom_navigation[BOTTOM_NAVIGATION_SIZE];

    // Middle part of the teksti tv seems to be only dynamic one
    html_row* middle;
    size_t middle_rows;

    html_buffer _curl_buffer;
    // Couldn't load the page
    bool curl_load_error;
    // Buffer for the loadable shortlink
    char link[HTML_LINK_SIZE];
} html_parser;

#define html_item_as_text(_item) ((_item).item.text)
#define html_item_as_link(_item) ((_item).item.link)

#define html_link_link(_link) ((_link).url.text)
#define html_link_text(_link) ((_link).inner_text.text)
#define html_link_text_size(_link) ((_link).inner_text.size)
#define html_text_text(_text) ((_text).text)
#define html_text_text_size(_text) ((_text).size)

void init_html_parser(html_parser* parser);
void free_html_parser(html_parser* parser);
void parse_html(html_parser* parser);
void link_from_ints(html_parser* parser, int page, int subpage);
void link_from_short_link(html_parser* parser, char* shortlink);

int page_number(const char* page);
int subpage_number(const char* subpage);

static inline size_t html_item_text_size(html_item item)
{
    switch (item.type) {
    case HTML_TEXT:
        return html_text_text_size(html_item_as_text(item));
    case HTML_LINK:
        return html_link_text_size(html_item_as_link(item));
    default:
        return 0;
    }
}

void load_page(html_parser* parser);

#endif
