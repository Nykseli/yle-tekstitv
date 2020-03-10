
#include <stdio.h>

#include "printer.h"

void print_title(html_parser* parser)
{
    printf("  %s\n", parser->title.text);
}

void print_middle(html_parser* parser)
{

    html_item_type last_type = HTML_TEXT;

    for (size_t i = 0; i < parser->middle_rows; i++) {
        printf("    ");
        for (size_t j = 0; j < parser->middle[i].size; j++) {
            html_item item = parser->middle[i].items[j];
            if (item.type == HTML_LINK) {
                if (last_type == HTML_LINK)
                    printf("-");

                printf(html_link_text(html_item_as_link(item)));
                last_type = HTML_LINK;
            } else if (item.type == HTML_TEXT) {
                printf(html_text_text(html_item_as_text(item)));
                last_type = HTML_TEXT;
            }
        }
        printf("\n");
    }
    printf("\n");
}

void print_parser(html_parser* parser)
{
    print_title(parser);
    print_middle(parser);
}