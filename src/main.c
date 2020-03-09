#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drawer.h"
#include "html_parser.h"
#include "page_loader.h"
#include "page_number.h"

int main(int argc, char** argv)
{

    if (argc > 3) {
        printf("Usage: %s [page] [sub page]\n", argv[0]);
        return 1;
    }

    if (argc == 2 && (strncmp(argv[1], "-h", 2) == 0 || strncmp(argv[1], "--help", 6) == 0)) {
        printf("Usage: %s [page] [sub page]\n", argv[0]);
        return 0;
    }

    int subpage = 1;
    int page = 100;
    if (argc >= 2) {
        page = page_number(argv[1]);
        if (page == -1) {
            printf("Page needs to be value between 100 and 999 (inclusive)\n");
            return 1;
        }
    }
    if (argc == 3) {
        subpage = subpage_number(argv[2]);
        if (subpage == -1) {
            printf("Sub page needs to be value between 1 and 99 (inclusive)\n");
            return 1;
        }
    }

    add_page(page);
    add_subpage(subpage);

    html_parser parser;
    init_html_parser(&parser);
    load_page(&parser, https_page_url);
    parse_html(&parser);

    drawer drawer;
    init_drawer(&drawer);
    draw_parser(&drawer, &parser);
    free_drawer(&drawer);

    free_html_parser(&parser);
    return 0;
}
