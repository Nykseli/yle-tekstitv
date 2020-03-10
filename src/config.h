#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdbool.h>

typedef struct {
    int page;
    int subpage;
    bool text_only;
    bool help;
    bool no_nav;
    bool no_top_nav;
    bool no_bottom_nav;
    bool no_title;
    bool no_middle;
} config;

void init_config(int argc, char** argv);

extern config global_config;

#endif
