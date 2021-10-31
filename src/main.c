#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tekstitv.h>

#include "config.h"
#include "drawer.h"
#include "printer.h"

static void print_usage(char* name)
{
    printf("Usage: %s [page] [sub page] [options]\n", name);
    printf("Options:\n");
    printf("\t-h,--help\t\tPrint this\n");
    printf("\t-t,--text-only\t\tPrint teletext to stdout instead using ncurses\n");
    printf("\t--help-config\t\tPrint config file options\n");
    printf("\t--version\t\tPrint program version\n");
    printf("\t--config <path>\t\tPath to config file. (Default: ~/.config/tekstitv/tekstitv.conf\n");
    printf("\t--bg-color <hex>\tBackground color hex value (000000)\n");
    printf("\t--text-color <hex>\tText color hex value (ffffff)\n");
    printf("\t--link-color <hex>\tLink highlight color hex value (ffffff)\n");
    printf("\t--navigation\t\tPrint navigation instructions\n");
    printf("\t--long-navigation\tPrint navigation instructions with extra information\n");
    printf("\t--no-nav\t\tDisable all navigations\n");
    printf("\t--no-top-nav\t\tDisable top navigation\n");
    printf("\t--no-bottom-nav\t\tDisable bottom navigation\n");
    printf("\t--no-title\t\tDisable title\n");
    printf("\t--no-middle\t\tDisable middle\n");
    printf("\t--no-sub-page\t\tDisable list of sub page numbers\n");
    printf("\t--default-colors\tDisable all custom coloring. Including custom link colors\n");
    printf("\t\t\t\tUse colors based on the console theme instead\n");
    printf("\t--show-time <format>\tShow time. Optional strftime format as argument.\n");
    exit(0);
}

static void print_config_options()
{
    printf("\ttext-only=true|false\t\tPrint teletext to stdout instead using ncurses\n");
    printf("\tno-nav=true|false\t\tDisable all navigations\n");
    printf("\tno-top-nav=true|false\t\tDisable top navigation\n");
    printf("\tno-bottom-nav=true|false\tDisable bottom navigation\n");
    printf("\tno-title=true|false\t\tDisable title\n");
    printf("\tno-middle=true|false\t\tDisable middle\n");
    printf("\tno-sub-page=true|false\t\tDisable list of sub page numbers\n");
    printf("\tdefault-colors=true|false\tDisable all custom coloring. Including link colors\n");
    printf("\t\t\t\t\tUse colors based on the console theme instead\n");
    printf("\tbg-color=<hex>\t\t\tBackground color hex value (000000)\n");
    printf("\ttext-color=<hex>\t\tText color hex value (ffffff)\n");
    printf("\tlink-color=<hex>\t\tLink highlight color hex value (ffffff)\n");
    printf("\tshow-time=false|true|format\tShow time. True is default format,\n");
    printf("\t\t\t\t\tformat arg is custom strftime format\n");
    exit(0);
}

static void print_version(char* name)
{
    printf("%s version %d.%d\n", name, TEKSTITV_MAJOR_VERSION, TEKSTITV_MINOR_VERSION);
    exit(0);
}

static void print_verbose_navigation_instructions()
{
    printf("Navigation instructions: \n\n");
    printf("|   Key   |         Action         |                         Info                        |\n");
    printf("|----------------------------------------------------------------------------------------|\n");
    printf("| j/Down  | Move one link down     | -                                                   |\n");
    printf("| k/Up    | Move one link up       | -                                                   |\n");
    printf("| l/Right | Move one link right    | -                                                   |\n");
    printf("| h/Left  | Move one link left     | -                                                   |\n");
    printf("| g/Enter | Open the selected link | -                                                   |\n");
    printf("| v       | Load previous page     | Tries to load page only if it exists                |\n");
    printf("| m       | Load next page         | Tries to load page only if it exists                |\n");
    printf("| b       | Load previous sub page | Tries to load page only if it exists                |\n");
    printf("| n       | Load next sub page     | Tries to load page only if it exists                |\n");
    printf("| s       | Search new page        | Automatically tries to load the page after 3 digits |\n");
    printf("| r       | Reload page            | -                                                   |\n");
    printf("| o       | Previous page          | -                                                   |\n");
    printf("| p       | Next page              | -                                                   |\n");
    printf("| i       | Show navigation help   | -                                                   |\n");
    printf("| esc     | Cancel search mode     | Works only in search mode                           |\n");
    printf("| q       | Quit program           | Works only if not in search mode                    |\n");
    printf("\n");
    exit(0);
}

static void print_navigation_instructions()
{
    printf("Navigation instructions: \n\n");
    printf("|   Key   |         Action         |\n");
    printf("|----------------------------------|\n");
    printf("| j/Down  | Move one link down     |\n");
    printf("| k/Up    | Move one link up       |\n");
    printf("| l/Right | Move one link right    |\n");
    printf("| h/Left  | Move one link left     |\n");
    printf("| g/Enter | Open the selected link |\n");
    printf("| v       | Load previous page     |\n");
    printf("| m       | Load next page         |\n");
    printf("| b       | Load previous sub page |\n");
    printf("| n       | Load next sub page     |\n");
    printf("| s       | Search new page        |\n");
    printf("| r       | Reload page            |\n");
    printf("| o       | Previous page          |\n");
    printf("| p       | Next page              |\n");
    printf("| i       | Show navigation help   |\n");
    printf("| esc     | Cancel search mode     |\n");
    printf("| q       | Quit program           |\n");
    printf("\n");
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

    if (global_config.long_navigation) {
        print_verbose_navigation_instructions();
    }

    if (global_config.navigation) {
        print_navigation_instructions();
    }

    if (global_config.help_config) {
        print_config_options();
    }

    if (global_config.page == -1) {
        printf("Page needs to be value between 100 and 999 (inclusive)\n");
        return 1;
    }
    if (global_config.subpage == -1) {
        printf("Sub page needs to be value between 1 and 99 (inclusive)\n");
        return 1;
    }

    html_parser parser;
    init_html_parser(&parser);
    link_from_ints(&parser, global_config.page, global_config.subpage);
    load_page(&parser);
    parse_html(&parser);

    if (global_config.text_only) {
        print_parser(&parser);
    } else {
        drawer drawer;
        init_drawer(&drawer);
        draw_parser(&drawer, &parser);
        free_drawer(&drawer);
    }

    free_config(&global_config);
    free_html_parser(&parser);
    return 0;
}
