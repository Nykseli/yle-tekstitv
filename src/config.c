
#include <string.h>

#include "config.h"
#include "page_number.h"

config global_config = {
    .page = 100,
    .subpage = 1,
    .text_only = false,
    .help = false,
    .version = false,
    .no_nav = false,
    .no_top_nav = false,
    .no_bottom_nav = false,
    .no_title = false,
    .no_middle = false
};

static void option_error(char* option)
{
    fprintf(stderr, "Unknown option: %s\n", option);
    exit(1);
}

static void long_option(char* option)
{
    if (strcmp(option, "--text-only") == 0) {
        global_config.text_only = true;
    } else if (strcmp(option, "--help") == 0) {
        global_config.help = true;
    } else if (strcmp(option, "--version") == 0) {
        global_config.version = true;
    } else if (strcmp(option, "--no-nav") == 0) {
        global_config.no_nav = true;
    } else if (strcmp(option, "--no-top-nav") == 0) {
        global_config.no_top_nav = true;
    } else if (strcmp(option, "--no-bottom-nav") == 0) {
        global_config.no_bottom_nav = true;
    } else if (strcmp(option, "--no-title") == 0) {
        global_config.no_title = true;
    } else if (strcmp(option, "--no-middle") == 0) {
        global_config.no_middle = true;
    } else {
        option_error(option);
    }
}

static void short_option(char* option)
{

    if (strcmp(option, "-t") == 0) {
        global_config.text_only = true;
    } else if (strcmp(option, "-h") == 0) {
        global_config.help = true;
    } else {
        option_error(option);
    }
}

void init_config(int argc, char** argv)
{
    bool page_found = false;
    bool sub_page_found = false;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == '-') {
                long_option(argv[i]);
            } else {
                short_option(argv[i]);
            }
        } else {
            if (!page_found) {
                global_config.page = page_number(argv[i]);
                page_found = true;
            } else if (!sub_page_found) {
                global_config.subpage = subpage_number(argv[i]);
                sub_page_found = true;
            } else {
                option_error(argv[i]);
            }
        }
    }
}
