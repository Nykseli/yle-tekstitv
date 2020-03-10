#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdbool.h>

typedef struct {
    int page;
    int subpage;
    bool text_only;
    bool help;
} config;

void init_config(int argc, char** argv);

extern config global_config;

#endif
