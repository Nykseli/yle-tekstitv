#ifndef _CONFIG_H_
#define _CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** Formated time based on the configured time format */
typedef struct {
    /** Time refers to a internal buffer so it should not be modified */
    const char* time;
    size_t time_len;
} fmt_time;

typedef struct {
    int page;
    int subpage;
    bool text_only;
    bool help;
    bool help_config;
    bool version;
    bool navigation;
    bool long_navigation;
    bool no_nav;
    bool no_top_nav;
    bool no_bottom_nav;
    bool no_title;
    bool no_middle;
    bool no_sub_page;
    bool default_colors;
    short bg_rgb[3];
    short link_rgb[3];
    short text_rgb[3];
    const char* time_fmt;

    // gui-only options
    int font_size;
    int w_width;
    int w_height;
    int w_x;
    int w_y;
    bool auto_refresh;
    int refresh_interval;
} config;

#define BG_RGB(i) (global_config.bg_rgb[i])
#define LINK_RGB(i) (global_config.link_rgb[i])
#define TEXT_RGB(i) (global_config.text_rgb[i])

void init_config(int argc, char** argv);
void free_config(config* conf);
void save_config();
fmt_time current_time();

/**
 * `name` in upadte functions refers to the option name in the config file
 */
void update_int_option(const char* name, int value);
void update_bool_option(const char* name, bool value);
void update_rgb_option(const char* name, uint8_t r, uint8_t g, uint8_t b);

extern config global_config;
// Ignores the config read from defalt path during config tests
extern bool ignore_config_read_during_testing;
extern int latest_config_exit_code;

#ifdef __cplusplus
}
#endif

#endif
