#ifndef _GUI_ICON_STRUCT_
#define _GUI_ICON_STRUCT_

#include <stdint.h>

#define GUI_ICON_DATA_MAX 4096

/**
 * Structure for the gui icons that are exported as c source code with gimp.
 */
typedef struct gui_icon_data {
    uint32_t width;
    uint32_t height;
    uint32_t bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
    /**
     * Size of the pixel data is caluclated by (width * height * bytes_per_pixel + 1).
     * Note that all the non transparent pixels need all have the same base color.
     * We want to avoid using too much stack and memory in general so limit the pixel data size
     */
    const uint8_t pixel_data[GUI_ICON_DATA_MAX];
} gui_icon_data;

#endif
