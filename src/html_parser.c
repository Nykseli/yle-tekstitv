
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "html_parser.h"

typedef enum {
    UNKNOWN,
    P,
    BIG,
    PRE,
    LINK,
    FONT,
    CENTER
} tag_type;

static inline void skip_next_str(html_buffer* buffer, const char* str, size_t len)
{
    // find the string
    for (; buffer->current < buffer->size; buffer->current++) {
        if (strncmp(buffer->html + buffer->current, str, len) == 0)
            break;
    }

    // actually skip the str
    buffer->current += len;
}

static inline void skip_next_char(html_buffer* buffer, const char c)
{
    // find the char
    for (; buffer->current < buffer->size; buffer->current++) {
        if (buffer->html[buffer->current] == c)
            break;
    }

    // actually skip the char
    buffer->current++;
}

static inline void skip_next_tag(html_buffer* buffer, const char* name, size_t name_len, bool closing)
{
    char tag_buff[64] = { '<' };
    int buf_len = 1;

    if (closing) {
        tag_buff[1] = '/';
        buf_len++;
    }

    strncpy(tag_buff + buf_len, name, name_len);
    buf_len += name_len;

    for (; buffer->current < buffer->size; buffer->current++) {
        if (strncmp(buffer->html + buffer->current, tag_buff, buf_len) == 0) {
            buffer->current += buf_len;
            break;
        }
    }

    skip_next_char(buffer, '>');
}

static tag_type get_tag_type(html_buffer* buffer)
{
    char c;
    size_t tag_len = 0;
    char* html = buffer->html + buffer->current;

    // Skip the possible tag opener
    if (*html == '<')
        html++;

    while ((c = html[tag_len]) != '\0' && c != '>' && c != ' ')
        tag_len++;

    if (tag_len == 1) {
        if (strncmp(html, "p", 1) == 0)
            return P;
        if (strncmp(html, "a", 1) == 0)
            return LINK;
    } else if (tag_len == 3) {
        if (strncmp(html, "big", 3) == 0)
            return BIG;
        if (strncmp(html, "pre", 3) == 0)
            return PRE;
    } else if (tag_len == 4) {
        if (strncmp(html, "font", 4) == 0)
            return FONT;
    } else if (tag_len == 6) {
        if (strncmp(html, "center", 6) == 0)
            return CENTER;
    }

    return UNKNOWN;
}

// Copy and filter html encoded characters
static size_t copy_html_text(char* target, char* src, size_t len)
{
    // filtered text
    unsigned char filter_buf[1024] = { 0 };
    size_t filter_len = 0;

    for (size_t i = 0; i < len; i++) {

        if (src[i] != '&') {
            filter_buf[filter_len] = src[i];
            filter_len++;
            continue;
        }

        i++;
        if (strncmp(src + i, "Auml;", 5) == 0) {
            // LATIN CAPITAL LETTER A WITH DIAERESIS
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0x84;
            filter_len += 2;
            i += 4;
        } else if (strncmp(src + i, "auml;", 5) == 0) {
            // LATIN SMALL LETTER A WITH DIAERESIS
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0xa4;
            filter_len += 2;
            i += 4;
        } else if (strncmp(src + i, "Atilde;", 7) == 0) {
            // LATIN CAPITAL LETTER A WITH TILDE
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0x83;
            filter_len += 2;
            i += 6;
        } else if (strncmp(src + i, "atilde;", 7) == 0) {
            // LATIN SMALL LETTER A WITH TILDE
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0xa3;
            filter_len += 2;
            i += 6;
        } else if (strncmp(src + i, "Ouml;", 5) == 0) {
            // LATIN CAPITAL LETTER O WITH DIAERESIS
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0x96;
            filter_len += 2;
            i += 4;
        } else if (strncmp(src + i, "ouml;", 5) == 0) {
            // LATIN SMALL LETTER A WITH DIAERESIS
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0xb6;
            filter_len += 2;
            i += 4;
        } else if (strncmp(src + i, "Aring;", 6) == 0) {
            // LATIN CAPITAL LETTER A WITH RING ABOVE
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0x85;
            filter_len += 2;
            i += 5;
        } else if (strncmp(src + i, "aring;", 6) == 0) {
            // LATIN SMALL LETTER A WITH RING ABOVE
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0xa5;
            filter_len += 2;
            i += 5;
        } else if (strncmp(src + i, "Uuml;", 5) == 0) {
            // LATIN CAPITAL LETTER U WITH DIAERESIS
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0x9c;
            filter_len += 2;
            i += 4;
        } else if (strncmp(src + i, "uuml;", 5) == 0) {
            // LATIN SMALL LETTER U WITH DIAERESIS
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0xbc;
            filter_len += 2;
            i += 4;
        } else if (strncmp(src + i, "Eacute;", 7) == 0) {
            // LATIN CAPITAL LETTER E WITH ACUTE
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0x89;
            filter_len += 2;
            i += 6;
        } else if (strncmp(src + i, "eacute;", 7) == 0) {
            // LATIN SMALL LETTER E WITH ACUTE
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0xa9;
            filter_len += 2;
            i += 6;
        } else if (strncmp(src + i, "curren;", 7) == 0) {
            // CURRENCY SIGN
            filter_buf[filter_len] = 0xc2;
            filter_buf[filter_len + 1] = 0xa4;
            filter_len += 2;
            i += 6;
        } else if (strncmp(src + i, "quot;", 5) == 0) {
            // QUOTATION MARK
            filter_buf[filter_len] = '"';
            filter_len += 1;
            i += 4;
        } else if (strncmp(src + i, "amp;", 4) == 0) {
            // AMPERSAND
            filter_buf[filter_len] = '&';
            filter_len += 1;
            i += 3;
        } else if (strncmp(src + i, "apos;", 5) == 0) {
            // APOSTROPHE
            filter_buf[filter_len] = '\'';
            filter_len += 1;
            i += 4;
        } else if (strncmp(src + i, "gt;", 3) == 0) {
            // GREATER-THAN SIGN
            filter_buf[filter_len] = '>';
            filter_len += 1;
            i += 2;
        } else if (strncmp(src + i, "lt;", 3) == 0) {
            // LESS-THAN SIGN
            filter_buf[filter_len] = '<';
            filter_len += 1;
            i += 2;
        }
    }

    strncpy(target, (char*)filter_buf, filter_len);
    return filter_len;
}

// parse current link in buffer to html_link
// inner_pre appends spaces before inner_text
static void parse_current_link(html_buffer* buffer, html_link* linkbuf, size_t inner_pre_space)
{
    // find the start of the link string
    skip_next_str(buffer, "href", 4);
    skip_next_char(buffer, '=');
    skip_next_char(buffer, '"');

    // copy the link. currently all the links are exactly 34 characters
    strncpy(linkbuf->url.text, buffer->html + buffer->current, HTML_LINK_SIZE);
    linkbuf->url.size = HTML_LINK_SIZE;
    linkbuf->url.text[HTML_LINK_SIZE] = '\0';

    // go to end of the opening a tag
    skip_next_char(buffer, '>');

    // find the length of the text
    size_t text_len = 0;
    for (;; text_len++) {
        if (buffer->html[buffer->current + text_len] == '<')
            break;
    }

    // copy the inner text
    // First add the possible pre_spaces
    for (size_t i = 0; i < inner_pre_space; i++)
        *(linkbuf->inner_text.text + i) = ' ';

    // Then copy the actual text
    size_t filter_len = copy_html_text(linkbuf->inner_text.text + inner_pre_space, buffer->html + buffer->current, text_len);
    linkbuf->inner_text.size = filter_len + inner_pre_space;
    linkbuf->inner_text.text[filter_len + inner_pre_space] = '\0';

    // go to end of the closing a tag
    skip_next_tag(buffer, "a", 1, true);
}

static void parse_title(html_parser* parser, html_buffer* buffer)
{
    skip_next_tag(buffer, "big", 3, false);
    size_t title_len = 0;
    for (;; title_len++) {
        if (buffer->html[buffer->current + title_len] == '<')
            break;
    }

    // Copy the title string
    strncpy(html_text_text(parser->title), buffer->html + buffer->current, title_len);
    parser->title.size = title_len;
    buffer->current += title_len;
    skip_next_tag(buffer, "big", 3, true);
}

static void parse_navigation_text(html_buffer* buffer, html_text* text, bool last_link)
{
    char endchar = last_link ? '<' : '&';
    size_t text_len = 0;
    for (;; text_len++) {
        if (buffer->html[buffer->current + text_len] == endchar)
            break;
    }

    strncpy(text->text, buffer->html + buffer->current, text_len);
    text->text[text_len] = '\0';
    text->size = text_len;
    buffer->current += text_len;
}

// Parse navigations for between next and previous (sub)pages
static void parse_top_navigation(html_parser* parser, html_buffer* buffer)
{
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        bool last_link = i == TOP_NAVIGATION_SIZE - 1;
        if (i != 0)
            skip_next_str(buffer, "&nbsp;", 6);

        tag_type tag = get_tag_type(buffer);
        if (tag == LINK) {
            parser->top_navigation[i].type = HTML_LINK;
            parse_current_link(buffer, &html_item_as_link(parser->top_navigation[i]), 0);
        } else {
            parser->top_navigation[i].type = HTML_TEXT;
            parse_navigation_text(buffer, &html_item_as_text(parser->top_navigation[i]), last_link);
        }

        if (!last_link)
            skip_next_str(buffer, "&nbsp;|", 7);
    }
}

static void parse_bottom_navigation(html_parser* parser, html_buffer* buffer)
{
    skip_next_tag(buffer, "p", 1, false);
    for (size_t i = 0; i < BOTTOM_NAVIGATION_SIZE; i++) {
        parse_current_link(buffer, &parser->bottom_navigation[i], 0);
    }
}
static void parse_middle_link(html_parser* parser, html_buffer* buffer, size_t spaces)
{
    // create new item
    html_item item;
    item.type = HTML_LINK;
    parse_current_link(buffer, &html_item_as_link(item), spaces);

    // append item to row
    size_t row_index = parser->middle[parser->middle_rows].size;
    parser->middle[parser->middle_rows].items[row_index] = item;
    parser->middle[parser->middle_rows].size++;
}

static void parse_middle_text(html_parser* parser, html_buffer* buffer, size_t spaces)
{

    size_t text_len = 0;
    for (;; text_len++) {
        char c = buffer->html[buffer->current + text_len];
        if (c == '<' || c == '\r')
            break;
    }

    // create new item
    html_item item;
    item.type = HTML_TEXT;

    // copy the text
    // First add the possible pre_spaces
    for (size_t i = 0; i < spaces; i++)
        item.item.text.text[i] = ' ';

    // Then copy the actual text
    //strncpy(item.item.text.text + spaces, buffer->html + buffer->current, text_len);
    size_t filter_len = copy_html_text(item.item.text.text + spaces, buffer->html + buffer->current, text_len);
    item.item.text.size = filter_len + spaces;
    item.item.text.text[item.item.text.size] = '\0';
    buffer->current += text_len;

    // append item to row
    size_t row_index = parser->middle[parser->middle_rows].size;
    parser->middle[parser->middle_rows].items[row_index] = item;
    parser->middle[parser->middle_rows].size++;
}

static void parse_middle(html_parser* parser, html_buffer* buffer)
{

    skip_next_tag(buffer, "pre", 3, false);
    // Loop until the closing pre tag is found
    for (; strncmp(buffer->html + buffer->current, "</pre>", 6) != 0; buffer->current++) {
        html_buffer line_buf;
        line_buf.current = 0;
        line_buf.size = 0;
        memset(line_buf.html, 0, 1028);

        for (; buffer->html[buffer->current] != '\n'; buffer->current++) {
            line_buf.html[line_buf.size] = buffer->html[buffer->current];
            line_buf.size++;
        }
        line_buf.html[line_buf.size] = '\0';

        if (line_buf.size == 0 || line_buf.html[0] == '&') {
            parser->middle_rows++;
            continue;
        }

        size_t pre_space = 0;
        for (; line_buf.current < line_buf.size && line_buf.html[line_buf.current] == ' '; line_buf.current++) {
            pre_space++;
        }

        line_buf.current--;
        for (int i = 0; line_buf.current < line_buf.size; line_buf.current++, i++) {
            tag_type type = get_tag_type(&line_buf);
            if (type == LINK) {
                parse_middle_link(parser, &line_buf, pre_space);
                line_buf.current--; // Don't miss the next starting character
            } else {
                parse_middle_text(parser, &line_buf, pre_space);
            }
            pre_space = 0;
        }

        parser->middle_rows++;
    }
}

void parse_html(html_parser* parser)
{
    html_buffer* buffer = &parser->_curl_buffer;
    buffer->current = 0;

    // Title
    skip_next_tag(buffer, "p", 1, false);
    parse_title(parser, buffer);
    skip_next_tag(buffer, "p", 1, true);

    // Top nav
    skip_next_tag(buffer, "SPAN", 4, false);
    parse_top_navigation(parser, buffer);
    skip_next_tag(buffer, "SPAN", 4, true);

    // Middle
    skip_next_tag(buffer, "DIV", 3, false);
    parse_middle(parser, buffer);
    skip_next_tag(buffer, "DIV", 3, true);

    // Bottom nav
    skip_next_tag(buffer, "DIV", 3, true);
    parse_bottom_navigation(parser, buffer);
}

void init_html_parser(html_parser* parser)
{
    parser->middle_rows = 0;
    parser->middle = calloc(MIDDLE_HTML_ROWS_MAX, sizeof(html_row));
    memset(parser->title.text, 0, HTML_TEXT_MAX);
    memset(parser->bottom_navigation, 0, sizeof(html_link) * BOTTOM_NAVIGATION_SIZE);
    memset(parser->top_navigation, 0, sizeof(html_item) * TOP_NAVIGATION_SIZE);
    memset(parser->_curl_buffer.html, 0, 1024 * 32);
}

void free_html_parser(html_parser* parser)
{
    if (parser->middle != NULL)
        free(parser->middle);
}
