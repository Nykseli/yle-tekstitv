#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdbool.h>

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
} config;

#define BG_RGB(i) (global_config.bg_rgb[i])
#define LINK_RGB(i) (global_config.link_rgb[i])
#define TEXT_RGB(i) (global_config.text_rgb[i])

void init_config(int argc, char** argv);

extern config global_config;

#endif
