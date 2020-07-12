
#include <string.h>

#include "config.h"
#include "page_number.h"

#define CURRENT (args.argv[args.current])
#define PREVIOUS (args.argv[args.current - 1])

// Helpers for parsing hex values
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_LOWERCASE(c) ((c) >= 'a' && (c) <= 'f')
#define IS_UPPERCASE(c) ((c) >= 'A' && (c) <= 'F')

typedef struct {
    int argc;
    int current;
    char** argv;
} arguments;

typedef enum {
    COLOR_BG,
    COLOR_LINK,
    COLOR_TEXT,
} color_type;

config global_config = {
    .page = 100,
    .subpage = 1,
    .text_only = false,
    .help = false,
    .version = false,
    .navigation = false,
    .long_navigation = false,
    .no_nav = false,
    .no_top_nav = false,
    .no_bottom_nav = false,
    .no_title = false,
    .no_middle = false,
    .bg_rgb = { -1, -1, -1 },
    .text_rgb = { -1, -1, -1 },
    .link_rgb = { -1, -1, -1 },
};

arguments args;

static void option_error()
{
    fprintf(stderr, "Unknown option: %s\n", CURRENT);
    exit(1);
}

static void color_parameter_error(bool empty)
{
    if (empty)
        fprintf(stderr, "%s argument needs hex color as an argument; was empty\n", PREVIOUS);
    else
        fprintf(stderr, "%s argument needs hex color as an argument; was %s\n", PREVIOUS, CURRENT);
    exit(1);
}

static short hex_to_short(const char* hex)
{
    int value = 0;
    if (IS_DIGIT(hex[0]))
        value += (hex[0] - '0') * 16;
    else if (IS_LOWERCASE(hex[0]))
        value += (hex[0] - 'a' + 10) * 16;
    else
        value += (hex[0] - 'A' + 10) * 16;

    if (IS_DIGIT(hex[1]))
        value += (hex[1] - '0');
    else if (IS_LOWERCASE(hex[1]))
        value += (hex[1] - 'a' + 10);
    else
        value += (hex[1] - 'A' + 10);

    return (short)(1000 * ((float)value / 255.0f));
}

static bool is_valid_hex(const char* hex)
{
    if (strlen(hex) != 6)
        return false;

    for (int i = 1; i < 6; i++) {
        char c = hex[i];
        if (!IS_DIGIT(c) && !IS_LOWERCASE(c) && !IS_UPPERCASE(c)) {
            return false;
        }
    }

    return true;
}

/**
 * Parse the hex value to match the representation of ncurses rgb values (0-1000)
 */
static bool hex_to_rgb(const char* hex, short* rbg)
{
    if (!is_valid_hex(hex))
        return false;

    rbg[0] = hex_to_short(hex);
    rbg[1] = hex_to_short(hex + 2);
    rbg[2] = hex_to_short(hex + 4);

    return true;
}

static void parse_color_argument(color_type color)
{
    args.current++;
    if (args.current >= args.argc)
        color_parameter_error(true);

    bool success;
    switch (color) {
    case COLOR_BG:
        success = hex_to_rgb(CURRENT, global_config.bg_rgb);
        break;
    case COLOR_LINK:
        success = hex_to_rgb(CURRENT, global_config.link_rgb);
        break;
    case COLOR_TEXT:
        success = hex_to_rgb(CURRENT, global_config.text_rgb);
        break;
    default:
        break;
    }

    if (!success)
        color_parameter_error(false);
}

static void long_option()
{
    if (strcmp(CURRENT, "--text-only") == 0) {
        global_config.text_only = true;
    } else if (strcmp(CURRENT, "--help") == 0) {
        global_config.help = true;
    } else if (strcmp(CURRENT, "--version") == 0) {
        global_config.version = true;
    } else if (strcmp(CURRENT, "--navigation") == 0) {
        global_config.navigation = true;
    } else if (strcmp(CURRENT, "--long-navigation") == 0) {
        global_config.long_navigation = true;
    } else if (strcmp(CURRENT, "--no-nav") == 0) {
        global_config.no_nav = true;
    } else if (strcmp(CURRENT, "--no-top-nav") == 0) {
        global_config.no_top_nav = true;
    } else if (strcmp(CURRENT, "--no-bottom-nav") == 0) {
        global_config.no_bottom_nav = true;
    } else if (strcmp(CURRENT, "--no-title") == 0) {
        global_config.no_title = true;
    } else if (strcmp(CURRENT, "--no-middle") == 0) {
        global_config.no_middle = true;
    } else if (strcmp(CURRENT, "--bg-color") == 0) {
        parse_color_argument(COLOR_BG);
    } else if (strcmp(CURRENT, "--text-color") == 0) {
        parse_color_argument(COLOR_TEXT);
    } else if (strcmp(CURRENT, "--link-color") == 0) {
        parse_color_argument(COLOR_LINK);
    } else {
        option_error();
    }
}

static void short_option()
{

    if (strcmp(CURRENT, "-t") == 0) {
        global_config.text_only = true;
    } else if (strcmp(CURRENT, "-h") == 0) {
        global_config.help = true;
    } else {
        option_error(CURRENT);
    }
}

void init_config(int argc, char** argv)
{
    bool page_found = false;
    bool sub_page_found = false;
    args.argc = argc;
    args.argv = argv;
    // We start from index 1 to skip the name of the program
    args.current = 1;

    for (; args.current < argc; args.current++) {
        if (argv[args.current][0] == '-') {
            if (argv[args.current][1] == '-') {
                long_option();
            } else {
                short_option();
            }
        } else {
            if (!page_found) {
                global_config.page = page_number(CURRENT);
                page_found = true;
            } else if (!sub_page_found) {
                global_config.subpage = subpage_number(CURRENT);
                sub_page_found = true;
            } else {
                option_error();
            }
        }
    }
}
