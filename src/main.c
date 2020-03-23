#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tekstitv.h>

#include "config.h"
#include "drawer.h"
#include "page_number.h"
#include "printer.h"

static void print_usage(char* name)
{
    printf("Usage: %s [page] [sub page] [options]\n", name);
    printf("Options:\n");
    printf("\t-h,--help\t\tPrint this\n");
    printf("\t-t,--text-only\t\tPrint teletext to stdout instead using ncurses\n");
    printf("\t--version\t\tPrint program version\n");
    printf("\t--no-nav\t\tDisable all navigations\n");
    printf("\t--no-top-nav\t\tDisable top navigation\n");
    printf("\t--no-bottom-nav\t\tDisable bottom navigation\n");
    printf("\t--no-title\t\tDisable title\n");
    printf("\t--no-middle\t\tDisable middle\n");
    exit(0);
}

static void print_version(char* name)
{
    printf("%s version %d.%d\n", name, TEKSTITV_MAJOR_VERSION, TEKSTITV_MINOR_VERSION);
    exit(0);
}

int main(int argc, char** argv)
{

    init_config(argc, argv);
    if (global_config.help) {
        print_usage(argv[0]);
    }

    if (global_config.version) {
        print_version(argv[0]);
    }

    if (global_config.page == -1) {
        printf("Page needs to be value between 100 and 999 (inclusive)\n");
        return 1;
    }
    if (global_config.subpage == -1) {
        printf("Sub page needs to be value between 1 and 99 (inclusive)\n");
        return 1;
    }

    add_page(global_config.page);
    add_subpage(global_config.subpage);

    html_parser parser;
    init_html_parser(&parser);
    load_page(&parser, https_page_url);
    parse_html(&parser);

    if (global_config.text_only) {
        print_parser(&parser);
    } else {
        drawer drawer;
        init_drawer(&drawer);
        draw_parser(&drawer, &parser);
        free_drawer(&drawer);
    }

    free_html_parser(&parser);
    return 0;
}
