
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <tekstitv.h>
#include <time.h>
#include <unistd.h>

#include "config.h"

#ifdef TESTING
#define exit(num)                        \
    do {                                 \
        latest_config_exit_code = (num); \
    } while (0)
#endif

#define CURRENT (args.argv[args.current])
#define PREVIOUS (args.argv[args.current - 1])
#define PEEK(amount) (args.argv[args.current + (amount)])

// Helpers for parsing hex values
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_LOWERCASE(c) ((c) >= 'a' && (c) <= 'f')
#define IS_UPPERCASE(c) ((c) >= 'A' && (c) <= 'F')

// Example 15.09. 10:05
#define DEFAULT_TIME_FMT "%d.%m. %H:%M"
#define TIME_BUFFER_SIZE 256
static char time_buffer[TIME_BUFFER_SIZE];

typedef struct {
    int argc;
    int current;
    char** argv;
} arguments;

typedef struct {
    char* start;
    char* end;
} config_arg;

typedef struct {
    config_arg option;
    config_arg parameter;
} config_line;

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
    .help_config = false,
    .version = false,
    .navigation = false,
    .long_navigation = false,
    .no_nav = false,
    .no_top_nav = false,
    .no_bottom_nav = false,
    .no_title = false,
    .no_middle = false,
    .no_sub_page = false,
    .default_colors = false,
    .bg_rgb = { -1, -1, -1 },
    .text_rgb = { -1, -1, -1 },
    .link_rgb = { -1, -1, -1 },
    .time_fmt = NULL
};

bool ignore_config_read_during_testing = false;
int latest_config_exit_code = 0;

arguments args;

// What line are we currenlty parsing in the config file
int config_file_num = 1;

static void option_error(void)
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

/**
 * random string is a optional parameter, if argument is not set
 * (no args or next one starts with '-'), the default_option is used.
 * This cannot fail so it will always return true
 */
static bool parse_default_option(char** option, const char* default_option)
{
    size_t arg_len;
    char* opt_copy;
    // Make sure that there is no douple allocs
    if (*option != NULL) {
        free(*option);
    }

    if (args.current + 1 >= args.argc) {
        goto use_default;
    }

    const char* next_arg = PEEK(1);
    if (next_arg[0] == '-') {
        goto use_default;
    }

    args.current++;
    arg_len = strlen(next_arg);
    opt_copy = (char*)malloc(arg_len + 1);
    strcpy(opt_copy, next_arg);
    opt_copy[arg_len] = '\0';
    *option = opt_copy;
    return true;

use_default:
    arg_len = strlen(default_option);
    opt_copy = (char*)malloc(arg_len + 1);
    strcpy(opt_copy, default_option);
    opt_copy[arg_len] = '\0';
    *option = opt_copy;
    return true;
}

static void long_option(void)
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
    } else if (strcmp(CURRENT, "--no-sub-page") == 0) {
        global_config.no_sub_page = true;
    } else if (strcmp(CURRENT, "--bg-color") == 0) {
        parse_color_argument(COLOR_BG);
    } else if (strcmp(CURRENT, "--text-color") == 0) {
        parse_color_argument(COLOR_TEXT);
    } else if (strcmp(CURRENT, "--link-color") == 0) {
        parse_color_argument(COLOR_LINK);
    } else if (strcmp(CURRENT, "--help-config") == 0) {
        global_config.help_config = true;
    } else if (strcmp(CURRENT, "--default-colors") == 0) {
        global_config.default_colors = true;
    } else if (strcmp(CURRENT, "--show-time") == 0) {
        parse_default_option((char**)&global_config.time_fmt, DEFAULT_TIME_FMT);
    } else if (strcmp(CURRENT, "--config") == 0) {
        // --config option is handled earlier so here we can just consume
        // the parameter and continue
        args.current++;
    } else {
        option_error();
    }
}

static void short_option(void)
{

    if (strcmp(CURRENT, "-t") == 0) {
        global_config.text_only = true;
    } else if (strcmp(CURRENT, "-h") == 0) {
        global_config.help = true;
    } else {
        option_error();
    }
}

static bool config_parse_error(const char* format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    fprintf(stderr, "Error parsing config file:\n");
    fprintf(stderr, "[line %d] ", config_file_num);
    vfprintf(stderr, format, argptr);
    fprintf(stderr, "\n");
    va_end(argptr);

    return false;
}

static bool valid_config_char(char c)
{
    // Since we now support random strings (like format, we can just check for all ascii chars)
    return (c >= '!' && c <= '~');
}

static bool is_white_space(char c)
{
    switch (c) {
    case ' ':
    case '\t':
    case '\r':
        return true;
    case '\n':
        config_file_num++;
        return true;
    default:
        break;
    }

    return false;
}

static void skip_white_space(char** data)
{
    for (; is_white_space(**data); (*data)++)
        ;
}

bool set_boolean_option(bool* option, char* param, int param_len)
{
    if (param_len == 4 && strncmp(param, "true", param_len) == 0) {
        *option = true;
    } else if (param_len == 4 && strncmp(param, "false", param_len) == 0) {
        *option = false;
    } else {
        return config_parse_error("Invalid boolean parameter.");
    }

    return true;
}

bool set_rbg_option(short* rgb, char* param, int param_len)
{
    if (param_len != 6)
        goto fail;

    // tmp char array so we can use hex_to_rgb
    char tmp_hex[] = { 0, 0, 0, 0, 0, 0, 0 };
    memcpy(tmp_hex, param, param_len);
    bool success = hex_to_rgb(tmp_hex, rgb);
    if (!success)
        goto fail;

    return true;

fail:
    return config_parse_error("Invalid hex color parameter.");
}

/**
 * "true" | "false" | "random string"
 * true sets the option to a the default string
 * false sets the option to NULL
 */
static bool set_default_option(char** option, char* param, int param_len, const char* default_option)
{
    // Make sure that there is no douple allocs
    if (*option != NULL) {
        free(*option);
    }

    if (param_len == 4 && strncmp(param, "true", param_len) == 0) {
        size_t opt_len = strlen(default_option);
        char* opt_copy = (char*)malloc(opt_len + 1);
        strncpy(opt_copy, default_option, opt_len);
        opt_copy[opt_len] = '\0';
        *option = opt_copy;
    } else if (param_len == 5 && strncmp(param, "false", param_len) == 0) {
        *option = NULL;
    } else {
        char* opt_copy = (char*)malloc(param_len + 1);
        strncpy(opt_copy, param, param_len);
        opt_copy[param_len] = '\0';
        *option = opt_copy;
    }

    return true;
}

static bool set_config_option(config_line line)
{
    // +1 because the counting starts from 0, so string with 2 chars would be for example
    // 1-0 = 1, even thouh it has 2 characters
    int option_len = line.option.end - line.option.start + 1;
    int parameter_len = line.parameter.end - line.parameter.start + 1;
    bool success = true;

    if (strncmp(line.option.start, "bg-color", option_len) == 0) {
        success = set_rbg_option(global_config.bg_rgb, line.parameter.start, parameter_len);
    } else if (strncmp(line.option.start, "text-color", option_len) == 0) {
        success = set_rbg_option(global_config.text_rgb, line.parameter.start, parameter_len);
    } else if (strncmp(line.option.start, "link-color", option_len) == 0) {
        success = set_rbg_option(global_config.link_rgb, line.parameter.start, parameter_len);
    } else if (strncmp(line.option.start, "text-only", option_len) == 0) {
        success = set_boolean_option(&global_config.text_only, line.parameter.start, parameter_len);
    } else if (strncmp(line.option.start, "no-nav", option_len) == 0) {
        success = set_boolean_option(&global_config.no_nav, line.parameter.start, parameter_len);
    } else if (strncmp(line.option.start, "no-top-nav", option_len) == 0) {
        success = set_boolean_option(&global_config.no_top_nav, line.parameter.start, parameter_len);
    } else if (strncmp(line.option.start, "no-bottom-nav", option_len) == 0) {
        success = set_boolean_option(&global_config.no_bottom_nav, line.parameter.start, parameter_len);
    } else if (strncmp(line.option.start, "no-title", option_len) == 0) {
        success = set_boolean_option(&global_config.no_title, line.parameter.start, parameter_len);
    } else if (strncmp(line.option.start, "no-middle", option_len) == 0) {
        success = set_boolean_option(&global_config.no_middle, line.parameter.start, parameter_len);
    } else if (strncmp(line.option.start, "no-sub-page", option_len) == 0) {
        success = set_boolean_option(&global_config.no_sub_page, line.parameter.start, parameter_len);
    } else if (strncmp(line.option.start, "default-colors", option_len) == 0) {
        success = set_boolean_option(&global_config.default_colors, line.parameter.start, parameter_len);
    } else if (strncmp(line.option.start, "show-time", option_len) == 0) {
        success = set_default_option((char**)&global_config.time_fmt, line.parameter.start, parameter_len, DEFAULT_TIME_FMT);
    } else {
        line.option.start[option_len] = '\0';
        return config_parse_error("Unknown option: '%s'", line.option.start);
    }

    return success;
}

// Don't make this static so tests can use this
bool parse_config_file(char* file_data)
{
    // Make sure that the config line starts from 1
    // This is mainly for tests to work correctly
    config_file_num = 1;

    config_line line;
    for (;;) {
        // First skip white space
        skip_white_space(&file_data);
        // Make sure that we are not at the end of the file
        if (*file_data == '\0')
            break;

        // Make sure that the line is not commented out
        if (*file_data == '#') {
            for (; *file_data != '\n'; file_data++)
                ;
            continue;
        }

        // Find the config option
        line.option.start = file_data;
        // Pointer to the last non-whitespace character
        char* last_opt_char = file_data;
        for (;; file_data++) {
            if (*file_data == '\n') {
                return config_parse_error("Character '=' expected after option.");
            }
            if (*file_data == '=' || *file_data == ' ' || *file_data == '\t')
                break;

            if (!valid_config_char(*file_data))
                return config_parse_error("Invalid character in option: '%c'", *file_data);

            last_opt_char = file_data;
        }

        line.option.end = last_opt_char;

        // Make sure that option was actually found
        if (line.option.start == line.option.end)
            return config_parse_error("Expected option before '='");

        // Find the '=' character
        skip_white_space(&file_data);
        if (*file_data != '=')
            return config_parse_error("Expected '=' after option");
        // Consume the '=' character
        file_data++;

        // Skip all whispace except newline and let the parameter finder handle it
        for (; *file_data == ' ' || *file_data == '\t'; file_data++)
            ;

        // Find the config option parameter
        line.parameter.start = file_data;
        // Pointer to the last non-whitespace character
        char* last_arg_char = file_data;
        for (;; file_data++) {
            if (*file_data == '\n') {
                break;
            }

            if (*file_data == ' ' || *file_data == '\t')
                continue;

            if (!valid_config_char(*file_data))
                return config_parse_error("Invalid character in parameter: '%c'", *file_data);

            last_arg_char = file_data;
        }

        line.parameter.end = last_arg_char;

        if (line.parameter.start == line.parameter.end)
            return config_parse_error("Expected parameter after '='");

        // Finally try to set the config option
        bool success = set_config_option(line);
        if (!success)
            return false;
    }

    return true;
}

/**
 * Load a config from path. Use NULL to load from default path (~/.config/tekstitv/tekstitv.conf)
 */
static void load_config(char* filepath)
{
    // Don't report missing file when trying to load the file from default path.
    bool default_config = false;
    char tmp_dir[1024];
    if (filepath == NULL) {
        // Let's hope the file path is under 1024 characters
        char* homedir;
        // Try to find the home path from $HOME variable
        // If and if not found, use the uid
        if ((homedir = getenv("HOME")) == NULL) {
            homedir = getpwuid(getuid())->pw_dir;
        }

        size_t homedir_len = strlen(homedir);
        if (homedir_len + 32 >= 1024) {
            fprintf(stderr, "Cannot open config. File path is over 1024 characters.");
            exit(1);
        }

        memcpy(tmp_dir, homedir, homedir_len);
        memcpy(tmp_dir + homedir_len, "/.config/tekstitv/tekstitv.conf", 32);
        tmp_dir[homedir_len + 32] = '\0';

        filepath = tmp_dir;
        default_config = true;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        if (errno == EPERM) {
            fprintf(stderr, "Cannot open file: %s. Permission denied.\n", filepath);
            exit(1);
        } else if (errno == ENOENT) {
            // Only report not found error if not loading the default config path.
            // We don't want the config to be required
            if (!default_config) {
                fprintf(stderr, "Cannot open file: %s. File doesn't exist.\n", filepath);
                exit(1);
            }
        } else {
            fprintf(stderr, "Unknown error while opening file: %s.\n", filepath);
            exit(1);
        }
        return;
    }

    // If all is good, we can try to load the config file and hand the data to our parser.
    struct stat fs;
    fstat(fd, &fs);

    char* buffer = malloc(fs.st_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read %s\n", filepath);
        exit(1);
    }

    read(fd, buffer, fs.st_size);
    buffer[fs.st_size] = '\0';

    bool success = parse_config_file(buffer);
    free(buffer);
    close(fd);
    if (!success)
        exit(1);
}

/**
 * Find if the config option is used and handle it.
 * If config option is not found. Try ~/.config/tekstitv/tekstitv.conf
 */
static void handle_config(void)
{
    int i = 1; // skip the program name
    bool config_found = false;
    for (; i < args.argc; i++) {
        if (strcmp(args.argv[i], "--config") == 0) {
            config_found = true;
            break;
        }
    }

    // Config option wasn't found so just use the default path
    if (!config_found) {
        load_config(NULL);
        return;
    }

    if (i == args.argc - 1) {
        fprintf(stderr, "--config argument needs file path as an argument; was empty\n");
        exit(1);
    }

    // Try to load the file given as an argument
    load_config(args.argv[i + 1]);
}

void init_config(int argc, char** argv)
{
    bool page_found = false;
    bool sub_page_found = false;
    args.argc = argc;
    args.argv = argv;
    // We start from index 1 to skip the name of the program
    args.current = 1;

    // Handle config option before other options.
    // This way the cli options override the config options.
    if (!ignore_config_read_during_testing)
        handle_config();

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

void free_config(config* conf)
{
    if (conf->time_fmt != NULL)
        free((char*)conf->time_fmt);
}

fmt_time current_time(void)
{
    fmt_time ctime;

    if (global_config.time_fmt == NULL) {
        ctime.time = "";
        ctime.time_len = 0;
        return ctime;
    }

    time_t ctimestamp = time(NULL);
    struct tm* current_time = localtime(&ctimestamp);
    size_t time_len = strftime(time_buffer, TIME_BUFFER_SIZE, global_config.time_fmt, current_time);

    ctime.time = time_buffer;
    ctime.time_len = time_len;
    return ctime;
}
