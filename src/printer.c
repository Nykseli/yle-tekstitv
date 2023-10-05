
#include <stdio.h>

#include "config.h"
#include "printer.h"

void print_time(int pre_padding)
{
    if (global_config.time_fmt == NULL)
        return;

    fmt_time ctime = current_time();

    // +4 because the middle prints get 4 spaces for padding
    size_t padding_len = MIDDLE_TEXT_MAX_LEN - pre_padding - ctime.time_len + 4;
    for (size_t ii = 0; ii < padding_len; ii++) {
        putc(' ', stdout);
    }

    printf("%s\n", ctime.time);
}

void print_title(html_parser* parser)
{
    if (global_config.no_title) {
        // Take the prepadding from the title print into account
        print_time(-2);
    } else {
        printf("  %s", parser->title.text);
        print_time(parser->title.size);
    }
}

void print_middle(html_parser* parser)
{
    if (global_config.no_middle)
        return;

    html_item_type last_type = HTML_TEXT;

    for (size_t i = 0; i < parser->middle_rows; i++) {
        for (size_t j = 0; j < parser->middle[i].size; j++) {
            html_item item = parser->middle[i].items[j];
            if (item.type == HTML_LINK) {
                if (last_type == HTML_LINK)
                    printf("-");

                printf("%s", html_link_text(html_item_as_link(item)));
                last_type = HTML_LINK;
            } else if (item.type == HTML_TEXT) {
                printf("%s", html_text_text(html_item_as_text(item)));
                last_type = HTML_TEXT;
            }
        }
        printf("\n");
    }
    printf("\n");
}

void print_parser(html_parser* parser)
{
    if (parser->curl_load_error) {
        printf("Couldn't load the page. Try another one\n");
        return;
    }

    print_title(parser);
    print_middle(parser);
}
