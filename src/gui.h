#ifndef _GUI_H_
#define _GUI_H_

// gui needs to be in c++ since we're going to use imgui that requires c++
#ifdef __cplusplus
extern "C" {
#endif

#include <tekstitv.h>

int display_gui(html_parser* parser);

#ifdef __cplusplus
}
#endif

#endif
