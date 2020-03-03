#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drawer.h"
#include "html_parser.h"
#include "page_loader.h"

static int page_number(const char* page)
{
    assert(page);

    size_t len = strlen(page);
    if (strlen(page) > 3)
        return -1;

    for (size_t i = 0; i < len; i++) {
        if (page[i] < '0' || page[i] > '9')
            return -1;
    }

    int result = atoi(page);
    return result >= 100 && result < 1000 ? result : -1;
}

static int subpage_number(const char* subpage)
{
    assert(subpage);

    size_t len = strlen(subpage);
    if (len > 2)
        return -1;

    for (size_t i = 0; i < len; i++) {
        if (subpage[i] < '0' || subpage[i] > '9')
            return -1;
    }

    int result = atoi(subpage);
    return result >= 1 && result < 100 ? result : -1;
}

static void add_page(int page, char* url)
{
    assert(url);
    assert(page >= 100 && page <= 999);

    // Pxxx part
    url[29] = (page / 100) + '0';
    url[30] = (page % 100 / 10) + '0';
    url[31] = (page % 10) + '0';
}

static void add_subpage(int subpage, char* url)
{
    assert(url);
    assert(subpage >= 1 && subpage <= 99);

    // _xx part
    url[33] = subpage > 9 ? (subpage % 100 / 10) + '0' : '0';
    url[34] = (subpage % 10) + '0';
}

int main(int argc, char** argv)
{
    static char url_template[] = "https://yle.fi/tekstitv/txt/Pxxx_xx.html";

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

    add_page(page, url_template);
    add_subpage(subpage, url_template);

    html_parser parser;
    init_html_parser(&parser);
    load_page(&parser, url_template);
    parse_html(&parser);

    drawer drawer;
    init_drawer(&drawer);
    draw_parser(&drawer, &parser);
    free_drawer(&drawer);

    free_html_parser(&parser);
    return 0;
}
