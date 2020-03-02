#ifndef _HTML_PARSER_H_
#define _HTML_PARSER_H_

#include <stddef.h>

#define HTML_TEXT_MAX 64
#define TOP_NAVIGATION_SIZE 4
#define BOTTOM_NAVIGATION_SIZE 12

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
    // title seems to always be 1 string
    html_text title;
    html_link top_navigation[TOP_NAVIGATION_SIZE];
    html_link bottom_navigation[BOTTOM_NAVIGATION_SIZE];

    // Middle part of the teksti tv seems to be only dynamic one
    html_item* middle;
    size_t middle_size;
} html_parser;

#define html_item_as_text(_item) ((_item).item.text)
#define html_item_as_link(_item) ((_item).item.link)

#define html_link_text(_link) ((_link).inner_text.text)
#define html_link_text_size(_link) ((_link).inner_text.size)
#define html_text_text(_text) ((_text).text)
#define html_text_text_size(_text) ((_text).size)

void init_html_parser(html_parser* parser);
void free_html_parser(html_parser* parser);
int parse_html(html_parser* parser, const char* html_text);

inline size_t html_item_text_size(html_item item)
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

#endif
