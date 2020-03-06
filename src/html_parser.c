
#include "tidy_std99.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "html_parser.h"

// What field of the teksti tv are we parsing
static int centers = 0;

typedef enum {
    UNKNOWN,
    P,
    BIG,
    PRE,
    LINK,
    FONT,
    CENTER
} tag_type;

static inline void skip_next_str(html_buffer* buffer, char* str, size_t len)
{
    // find the string
    for (; buffer->current < buffer->size; buffer->current++) {
        if (strncmp(buffer->html + buffer->current, str, len) == 0)
            break;
    }

    // actually skip the str
    buffer->current += len;
}

static inline void skip_next_char(html_buffer* buffer, char c)
{
    // find the char
    for (; buffer->current < buffer->size; buffer->current++) {
        if (buffer->html[buffer->current] == c)
            break;
    }

    // actually skip the char
    buffer->current++;
}

static inline void skip_next_tag(html_buffer* buffer, char* name, size_t name_len, bool closing)
{
    skip_next_char(buffer, '<');
    if (closing)
        skip_next_char(buffer, '/');

    skip_next_str(buffer, name, name_len);
    skip_next_char(buffer, '>');
}

tag_type get_tag_type(html_buffer* buffer)
{
    char c;
    size_t tag_len = 0;
    char* html = buffer->html + buffer->current;

    // Skip the possible tag opener
    if (*html == '<')
        html++;

    while ((c = html[tag_len]) != EOF && c != '>' && c != ' ')
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

// move buffer->current to next <center>
void center_start(html_buffer* buffer)
{
    for (; buffer->current < buffer->size; buffer->current++) {
        char c = buffer->html[buffer->current];
        if (c != '<')
            continue;

        buffer->current++;
        tag_type type = get_tag_type(buffer);
        if (type != CENTER)
            continue;

        // Go to end of the tag
        for (; buffer->html[buffer->current] != '>'; buffer->current++) {
        }

        // skip the >
        buffer->current++;
        break;
    }
}

// parse next link in buffer to html_link
// inner_pre appends spaces before inner_text
void parse_next_link(html_buffer* buffer, html_link* linkbuf, size_t inner_pre_space)
{
    // find next a tag
    for (; buffer->current < buffer->size; buffer->current++) {
        if (get_tag_type(buffer) == LINK)
            break;
    }

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
    strncpy(linkbuf->inner_text.text + inner_pre_space, buffer->html + buffer->current, text_len);
    linkbuf->inner_text.size = text_len + inner_pre_space;
    linkbuf->inner_text.text[text_len + inner_pre_space] = '\0';

    // go to end of the closing a tag
    skip_next_tag(buffer, "a", 1, true);
}

void parse_title(html_parser* parser, html_buffer* buffer)
{
    // find end of big tag after pre
    for (size_t i = 0; i < 2; i++) {
        skip_next_char(buffer, '>');
    }

    size_t title_len = 0;
    for (;; title_len++) {
        if (buffer->html[buffer->current + title_len] == '<')
            break;
    }

    // Copy the title string
    strncpy(html_text_text(parser->title), buffer->html + buffer->current, title_len);
    parser->title.size = title_len;
    buffer->current += title_len;
}

void parse_top_navigation(html_parser* parser, html_buffer* buffer)
{
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        // TODO: do memset in init_html_buffer
        memset(parser->top_navigation + i, 0, HTML_TEXT_MAX * 4);
        parse_next_link(buffer, parser->top_navigation + i, 0);
    }
}

void parse_bottom_navigation(html_parser* parser, html_buffer* buffer)
{
    // TODO: memset in html_parser init
    memset(parser->bottom_navigation, 0, sizeof(html_row) * BOTTOM_NAVIGATION_SIZE);

    // Parse the two navigation rows
    skip_next_tag(buffer, "p", 1, false);
    // First line is 4 links
    for (size_t i = 0; i < 4; i++) {
        parse_next_link(buffer, &html_item_as_link(parser->bottom_navigation[0].items[i]), 0);
        parser->bottom_navigation[0].size++;
    }

    skip_next_tag(buffer, "p", 1, false);
    // Second line is 6 links
    for (size_t i = 0; i < 6; i++) {
        parse_next_link(buffer, &html_item_as_link(parser->bottom_navigation[1].items[i]), 0);
        parser->bottom_navigation[1].size++;
    }
}
void parse_middle_link(html_parser* parser, html_buffer* buffer, size_t spaces)
{
    // create new item
    html_item item;
    item.type = HTML_LINK;
    parse_next_link(buffer, &html_item_as_link(item), spaces);

    // append item to row
    size_t row_index = parser->middle[parser->middle_rows].size;
    parser->middle[parser->middle_rows].items[row_index] = item;
    parser->middle[parser->middle_rows].size++;
}

// Copy and filter html encoded ä and ö characters
size_t copy_middle_text(char* target, char* src, size_t len)
{
    // filtered text
    unsigned char filter_buf[1024] = { 0 };
    size_t filter_len = 0;

    for (size_t i = 0; i < len; i++) {
        //    printf("%c\n", src[i]);
        if (src[i] != '&') {
            filter_buf[filter_len] = src[i];
            filter_len++;
            continue;
        }

        i++;
        switch (src[i]) {
        // ä chacter utf-8 ncurses doesn't like Ä
        case 'a':
        case 'A':
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0xa4;
            break;
        // ö chacter utf-8 ncurses doesn't like Ö
        case 'o':
        case 'O':
            filter_buf[filter_len] = 0xc3;
            filter_buf[filter_len + 1] = 0xb6;
            break;
        default:
            // error?
            break;
        }
        filter_len += 2;
        i += 4;
    }

    strncpy(target, (char*)filter_buf, filter_len);
    return filter_len;
}

void parse_middle_text(html_parser* parser, html_buffer* buffer, size_t spaces)
{
    // Sometimes <big> is lost in font
    if (strncmp(buffer->html + buffer->current, " <big>", 6) == 0) {
        buffer->current += 6;
        spaces += 2;
    }

    size_t text_len = 0;
    for (;; text_len++) {
        if (buffer->html[buffer->current + text_len] == '<')
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
    size_t filter_len = copy_middle_text(item.item.text.text + spaces, buffer->html + buffer->current, text_len);
    item.item.text.size = filter_len + spaces;
    item.item.text.text[filter_len] = '\0';
    buffer->current += text_len - 1;

    // append item to row
    size_t row_index = parser->middle[parser->middle_rows].size;
    parser->middle[parser->middle_rows].items[row_index] = item;
    parser->middle[parser->middle_rows].size++;
}

void parse_middle_font(html_parser* parser, html_buffer* line_buf, size_t spaces)
{
    skip_next_tag(line_buf, "font", 4, false);

    /* parse_middle_next_item(parser, line_buf, spaces); */
    for (; line_buf->current < line_buf->size && strncmp(line_buf->html + line_buf->current, "</font>", 7) != 0; line_buf->current++) {

        switch (get_tag_type(line_buf)) {
        case LINK:
            parse_middle_link(parser, line_buf, spaces);
            line_buf->current--;
            break;
        case UNKNOWN: // No tag found means there is text to be found
            parse_middle_text(parser, line_buf, spaces);
            break;
        default:
            break;
        }
        spaces = 0;
    }
    skip_next_tag(line_buf, "font", 4, true);
    // Sometimes </big> is lost after font
    if (strncmp(line_buf->html + line_buf->current, " </big>", 7) == 0) {
        line_buf->current += 7;
    }
}

void parse_middle_big(html_parser* parser, html_buffer* line_buf, size_t spaces)
{
    skip_next_tag(line_buf, "big", 3, false);
    /* parse_middle_next_item(parser, line_buf, spaces); */
    for (; line_buf->current < line_buf->size && strncmp(line_buf->html + line_buf->current, "</big>", 6) != 0; line_buf->current++) {

        switch (get_tag_type(line_buf)) {
        case LINK:
            parse_middle_link(parser, line_buf, spaces);
            line_buf->current--;
            break;
        case FONT:
            parse_middle_font(parser, line_buf, spaces);
            break;
        case UNKNOWN: // No tag found means there is text to be found
            parse_middle_text(parser, line_buf, spaces);
            break;
        default:
            break;
        }
        spaces = 0;
    }
    skip_next_tag(line_buf, "big", 3, true);
}

void parse_middle(html_parser* parser, html_buffer* buffer)
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

        if (line_buf.size == 0) {
            parser->middle_rows++;
            continue;
        }

        size_t pre_space = 0;
        for (; line_buf.current < line_buf.size && line_buf.html[line_buf.current] == ' '; line_buf.current++) {
            pre_space++;
        }

        for (; line_buf.current < line_buf.size; line_buf.current++) {
            tag_type type = get_tag_type(&line_buf);
            if (type == FONT) {
                parse_middle_font(parser, &line_buf, pre_space);
            } else if (type == BIG) {
                parse_middle_big(parser, &line_buf, pre_space);
            }
        }

        parser->middle_rows++;
    }
}

void parse_html(html_parser* parser)
{
    html_buffer* buffer = &parser->_curl_buffer;
    buffer->current = 0;
    for (; buffer->current < buffer->size; buffer->current++) {

        // Go to next center tag
        skip_next_tag(buffer, "center", 6, false);
        if (centers == 0) {
            parse_title(parser, buffer);
        } else if (centers == 1) {
            parse_top_navigation(parser, buffer);
        } else if (centers == 2) {
            parse_middle(parser, buffer);
        } else if (centers == 3) {
            parse_bottom_navigation(parser, buffer);
        }
        // Go to </center>
        skip_next_tag(buffer, "center", 6, true);
        centers++;
    }
}

void init_html_parser(html_parser* parser)
{

    //parser->middle = malloc(MIDDLE_HTML_ROWS_MAX * sizeof(html_row));
    parser->middle = calloc(MIDDLE_HTML_ROWS_MAX, sizeof(html_row));
    for (size_t i = 0; i < MIDDLE_HTML_ROWS_MAX; i++) {
        parser->middle[i].size = 0;
    }
    //open_html("P100_01.html", &parser->_curl_buffer);

    parser->middle_rows = 0;
    memset(parser->title.text, 0, HTML_TEXT_MAX);
    memset(parser->bottom_navigation, 0, sizeof(html_row) * BOTTOM_NAVIGATION_SIZE);
    memset(parser->top_navigation, 0, sizeof(html_link) * TOP_NAVIGATION_SIZE);
}

void free_html_parser(html_parser* parser)
{
    if (parser->middle != NULL)
        free(parser->middle);
}
