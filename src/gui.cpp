#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <SDL.h>
#include <SDL_ttf.h>
#include <imgui.h>
#include <imgui/backends/imgui_impl_sdl.h>
#include <imgui/backends/imgui_impl_sdlrenderer.h>

#include "gui.h"
#include "util.h"

#include <arrow_icon.h>
#include <home_icon.h>
#include <icon_struct.h>

extern "C" {
#include "config.h"
}

typedef struct imgui_settings {
    // We need float vectors so we can edit the colors with imgui controls
    ImVec4 ebg_color;
    ImVec4 etext_color;
    ImVec4 elink_color;

    bool show_settings;
} imgui_settings;

/**
 * gui_text contains data for each visible text object on the window
 */
typedef struct gui_text {
    SDL_Rect rect;
    SDL_Texture* texture;
    /**
     * gui text can be an icon when the icon doesn't contain a link
     */
    double icon_angle;
    /**
     * Not null if text is a icon
     */
    gui_icon_data* icon;
} gui_text;

/**
 * gui_link contains data for each visible link object on the window
 * and short link to teletext so it can be easily loaded
 */
typedef struct gui_link {
    SDL_Rect rect;
    SDL_Texture* texture;
    /**
     * short link to the link from parser, should not be modified
     */
    const char* short_link;
    /**
     * save inner_text so it can be easily rerendered
     */
    const char* inner_text;
    bool highlighted;
    /**
     * Not null if link is a icon
     */
    gui_icon_data* icon;
    /**
     * Optional angle for rotating the texture if we're using icon
     */
    double icon_angle;
} gui_link;

typedef struct gui_drawer {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    int line_height; // height of a single character
    int char_width; // width of a single character

    // title is: page, title and time
    gui_text title[3];

    // if error texture is not null, error has happened
    gui_text error;

    gui_text* texts;
    int text_count;

    gui_link* links;
    int link_count;

    int w_width;
    int w_height;
    int current_x;
    int current_y;

    int font_size;
    SDL_Color bg_color;
    SDL_Color text_color;
    SDL_Color link_color;

    bool auto_refresh;
    int refresh_interval;
    // keep track how much time has passed since last refresh
    int refresh_timer;

    char current_page[4];

    // settings for imgui specific things
    imgui_settings imgui;
} gui_drawer;

// milliseconds between redraws
// 16ms is roughly 60 fps
#define REDRW_DELAY 16

// TODO: how to bundle up the font to the binary?
const char* font_path = "assets/test_font.ttf";

static void load_new_page(gui_drawer* drawer, html_parser* parser, bool add_history);

bool is_window_maximized(SDL_Window* window)
{
    Uint32 flags = SDL_GetWindowFlags(window);
    return (flags & SDL_WINDOW_MAXIMIZED) == SDL_WINDOW_MAXIMIZED;
}

bool is_sdl_colors_equal(SDL_Color a, SDL_Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

int icon_size(gui_drawer* drawer)
{
    return drawer->font_size * 1.5;
}

ImVec4 sdl_color_to_imvec4(SDL_Color* src)
{

    Uint8 r = src->r;
    Uint8 g = src->g;
    Uint8 b = src->b;
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.00f);
}

SDL_Color config_rgb_to_color(short* rgb)
{
#define SHORT_8BIT(s) (Uint8)(255 * (s / 1000.0f))

    Uint8 r = SHORT_8BIT(rgb[0]);
    Uint8 g = SHORT_8BIT(rgb[1]);
    Uint8 b = SHORT_8BIT(rgb[2]);

    SDL_Color color = { r, g, b, 0 };
    return color;

#undef SHORT_8BIT
}

SDL_Color config_rgb_to_def_color(short* rgb, SDL_Color def)
{
    if (*rgb == -1 || global_config.default_colors) {
        return def;
    }

    return config_rgb_to_color(rgb);
}

SDL_Surface* create_icon_surface(gui_drawer* drawer, gui_icon_data* icon, bool link, bool highlight = false)
{

    Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#error "BIG_ENDIAN is not supported"
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

    // The icon data is read-only so we need to create a local copy of it
    uint32_t data_size = icon->bytes_per_pixel * icon->width * icon->height + 1;
    uint8_t pixel_copy[GUI_ICON_DATA_MAX];
    memcpy(pixel_copy, icon->pixel_data, data_size);

    SDL_Color icon_color = drawer->link_color;
    SDL_Color icon_bg = drawer->bg_color;

    if (!link) {
        icon_color = drawer->text_color;
    } else if (highlight) {
        icon_color = drawer->bg_color;
        icon_bg = drawer->link_color;
    }

    for (uint32_t ii = 0; ii < data_size; ii += 4) {
        uint8_t* pixel = pixel_copy + ii;
        // 4th pixel is always the alpha
        // TODO: does this work on a big endian machine?
        if (pixel[3] > 0) {
            pixel[0] = icon_color.r;
            pixel[1] = icon_color.g;
            pixel[2] = icon_color.b;
        } else {
            pixel[0] = icon_bg.r;
            pixel[1] = icon_bg.g;
            pixel[2] = icon_bg.b;
            pixel[3] = UINT8_MAX;
        }
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        (void*)pixel_copy,
        icon->width,
        icon->height,
        icon->bytes_per_pixel * 8,
        icon->bytes_per_pixel * icon->width,
        rmask,
        gmask,
        bmask,
        amask);

    return surface;
}

/**
 * This we want to save to the config file
 */
void set_gui_options_to_be_saved(gui_drawer* drawer)
{
    if (drawer->font_size != global_config.font_size)
        update_int_option("font-size", drawer->font_size);

    int window_w, window_h;
    SDL_GetWindowSize(drawer->window, &window_w, &window_h);
    if (window_h != global_config.w_height)
        update_int_option("window-height", window_h);
    if (window_w != global_config.w_width)
        update_int_option("window-width", window_w);

    int window_x, window_y;
    SDL_GetWindowPosition(drawer->window, &window_x, &window_y);
    if (window_y != global_config.w_x)
        update_int_option("window-x", window_x);
    if (window_x != global_config.w_y)
        update_int_option("window-y", window_y);

#define _m_update_color(name, c) update_rgb_option(name, c.r, c.g, c.b)
    if (!is_sdl_colors_equal(drawer->bg_color, config_rgb_to_color(global_config.bg_rgb))) {
        _m_update_color("bg-color", drawer->bg_color);
    }
    if (!is_sdl_colors_equal(drawer->text_color, config_rgb_to_color(global_config.text_rgb))) {
        _m_update_color("text-color", drawer->text_color);
    }
    if (!is_sdl_colors_equal(drawer->link_color, config_rgb_to_color(global_config.link_rgb))) {
        _m_update_color("link-color", drawer->link_color);
    }
#undef _m_update_color

    if (drawer->auto_refresh != global_config.auto_refresh)
        update_bool_option("auto-refresh", drawer->auto_refresh);
    if (drawer->refresh_interval != global_config.refresh_interval)
        update_int_option("refresh-interval", drawer->refresh_interval);

    bool fullscreen = is_window_maximized(drawer->window);
    if (fullscreen != global_config.fullscreen)
        update_bool_option("fullscreen", fullscreen);
}

void set_config(gui_drawer* drawer)
{
    SDL_Color bgColor = { 0, 0, 0, 0 };
    SDL_Color textColor = { 255, 255, 255, 0 };
    SDL_Color linkColor = { 17, 159, 244, 0 };

    drawer->bg_color = config_rgb_to_def_color(global_config.bg_rgb, bgColor);
    drawer->text_color = config_rgb_to_def_color(global_config.text_rgb, textColor);
    drawer->link_color = config_rgb_to_def_color(global_config.link_rgb, linkColor);

    drawer->imgui.ebg_color = sdl_color_to_imvec4(&drawer->bg_color);
    drawer->imgui.etext_color = sdl_color_to_imvec4(&drawer->text_color);
    drawer->imgui.elink_color = sdl_color_to_imvec4(&drawer->link_color);
    drawer->imgui.show_settings = false;

    drawer->font_size = global_config.font_size;
    drawer->w_width = global_config.w_width;
    drawer->w_height = global_config.w_height;

    drawer->auto_refresh = global_config.auto_refresh;
    drawer->refresh_interval = global_config.refresh_interval;
    drawer->refresh_timer = 0;
}

void set_gui_time_fmt(char** fmt_target)
{
#define GUI_TIME_FMT "%d.%m. %H:%M:%S"
    *fmt_target = (char*)malloc(strlen(GUI_TIME_FMT) + 1);
    strcpy(*fmt_target, GUI_TIME_FMT);
}

void set_font(gui_drawer* drawer)
{
    if (drawer->font != NULL) {
        TTF_CloseFont(drawer->font);
    }

    drawer->font = TTF_OpenFont(font_path, drawer->font_size);
    if (drawer->font == NULL) {
        fprintf(stderr, "error: font not found\n");
        exit(EXIT_FAILURE);
    }
}

void init_gui_drawer(gui_drawer* drawer)
{
    // Initialise the structure
    drawer->window = NULL;
    drawer->renderer = NULL;
    drawer->texts = NULL;
    drawer->links = NULL;
    drawer->font = NULL;
    drawer->char_width = 0;
    drawer->line_height = 0;
    drawer->text_count = 0;
    drawer->link_count = 0;
    drawer->w_width = 0;
    drawer->w_height = 0;
    drawer->current_x = 0;
    drawer->current_y = 0;
    drawer->error.texture = NULL;
    sprintf(drawer->current_page, "%d", global_config.page);
    // Always display time when in gui mode
    if (global_config.time_fmt == NULL) {
        set_gui_time_fmt((char**)&global_config.time_fmt);
    }

    for (int ii = 0; ii < 3; ii++) {
        drawer->title[ii].texture = NULL;
    }

    set_config(drawer);

    // Initialize SDL
    int x = global_config.w_x != INT32_MIN ? global_config.w_x : SDL_WINDOWPOS_UNDEFINED;
    int y = global_config.w_y != INT32_MIN ? global_config.w_y : SDL_WINDOWPOS_UNDEFINED;

    // set the needed flags when creating the window to make the opening
    // of the application more smooth
    Uint32 wflags = 0;
    if (global_config.fullscreen)
        wflags |= SDL_WINDOW_MAXIMIZED;

    drawer->window = SDL_CreateWindow(NULL, x, y, drawer->w_width, drawer->w_height, wflags);
    drawer->renderer = SDL_CreateRenderer(drawer->window, -1, 0);

    // window settings need to be set after the creation
    SDL_SetWindowResizable(drawer->window, SDL_TRUE);
    SDL_SetWindowMinimumSize(drawer->window, 40, 40);
}

void free_render_texts(gui_drawer* drawer)
{
    if (drawer->texts != NULL) {
        for (int ii = 0; ii < drawer->text_count; ii++) {
            SDL_DestroyTexture(drawer->texts[ii].texture);
        }
        free(drawer->texts);
    }

    if (drawer->links != NULL) {
        for (int ii = 0; ii < drawer->link_count; ii++) {
            SDL_DestroyTexture(drawer->links[ii].texture);
        }
        free(drawer->links);
    }

    if (drawer->error.texture != NULL) {
        SDL_DestroyTexture(drawer->error.texture);
        drawer->error.texture = NULL;
    }

    for (int ii = 0; ii < 3; ii++) {
        if (drawer->title[ii].texture != NULL) {
            SDL_DestroyTexture(drawer->title[ii].texture);
            drawer->title[ii].texture = NULL;
        }
    }

    drawer->char_width = 0;
    drawer->line_height = 0;
    drawer->text_count = 0;
    drawer->link_count = 0;
    drawer->current_x = 0;
    drawer->current_y = 0;
    drawer->texts = NULL;
    drawer->links = NULL;
}

void free_gui_drawer(gui_drawer* drawer)
{
    if (drawer->window != NULL) {
        SDL_DestroyWindow(drawer->window);
    }

    if (drawer->renderer != NULL) {
        SDL_DestroyRenderer(drawer->renderer);
    }

    free_render_texts(drawer);
}

/**
 * Calculate the x position where `text_length` sized string should start
 */
int calc_middle_x(gui_drawer* drawer, int text_length)
{
    int window_w, window_h;
    SDL_GetWindowSize(drawer->window, &window_w, &window_h);

    // TODO: Can we use floats somehow?
    // we need to multiple with char_widht to get the pixel count
    return window_w / 2 - (text_length * drawer->char_width) / 2;
}

const char* check_link_click(gui_drawer* drawer)
{
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    for (int ii = 0; ii < drawer->link_count; ii++) {
        gui_link link = drawer->links[ii];
        int lx = link.rect.x;
        int ly = link.rect.y;
        int lw = link.rect.w;
        int lh = link.rect.h;

        // is mouse on top of the link rectangle
        if (mx >= lx && mx <= lx + lw && my >= ly && my <= ly + lh) {
            return link.short_link;
        }
    }

    return NULL;
}

bool check_mouse_click(gui_drawer* drawer, html_parser* parser, SDL_Event* event)
{
    const char* link = NULL;
    bool add_history = false;

    Uint8 button = event->button.button;
    switch (button) {
    case 1:
        link = check_link_click(drawer);
        add_history = true;
        break;
    case 4: // previous mouse button
        link = history_prev_link();
        break;
    case 5: // next mouse button
        link = history_next_link();
        break;
    default:
        break;
    }

    if (link != NULL) {
        link_from_short_link(parser, (char*)link);
        load_new_page(drawer, parser, add_history);
        return true;
    }

    return false;
}

int set_gui_text_texture(gui_drawer* drawer, gui_text* new_text, const char* text, gui_icon_data* icon = NULL, double angle = 0.0)
{
    SDL_Surface* surface;
    int text_width;
    int text_height;

    if (icon == NULL) {
        surface = TTF_RenderUTF8_Shaded(drawer->font, text, drawer->text_color, drawer->bg_color);
        text_width = surface->w;
        text_height = surface->h;
    } else {
        surface = create_icon_surface(drawer, icon, false);
        text_width = icon_size(drawer);
        text_height = icon_size(drawer);
    }

    new_text->texture = SDL_CreateTextureFromSurface(drawer->renderer, surface);
    SDL_FreeSurface(surface);
    new_text->rect.x = drawer->current_x;
    new_text->rect.y = drawer->current_y;
    new_text->rect.w = text_width;
    new_text->rect.h = text_height;
    new_text->icon = icon;
    new_text->icon_angle = angle;

    return text_width;
}

void add_error_message(gui_drawer* drawer, const char* text)
{
    int text_length = strlen(text);
    // Set the message in the middle of the window
    int window_w, window_h;
    SDL_GetWindowSize(drawer->window, &window_w, &window_h);
    drawer->current_x = window_w / 2 - (text_length * drawer->char_width) / 2;
    drawer->current_y = window_h / 2 - drawer->line_height / 2;
    set_gui_text_texture(drawer, &drawer->error, text);
}

/**
 * Add a new text texture, texture will be added to the current rectangle
 * of the drawer and the current x pos is updated to the end of the text
 */
void add_text_texture(gui_drawer* drawer, const char* text, gui_icon_data* icon = NULL, double angle = 0.0)
{
    if (drawer->texts == NULL) {
        drawer->texts = (gui_text*)malloc(sizeof(gui_text));
    } else {
        drawer->texts = (gui_text*)realloc(drawer->texts, sizeof(gui_text) * (drawer->text_count + 1));
    }

    gui_text* new_text = &drawer->texts[drawer->text_count];
    drawer->text_count++;

    drawer->current_x += set_gui_text_texture(drawer, new_text, text, icon, angle);
}

/**
 * Add a new text or icon link texture, texture will be added to the current rectangle
 * of the drawer and the current x pos is updated to the end of the text.
 * If icon is not NULL, the texture surface is created from the icon
 */
void add_clickable_texture(gui_drawer* drawer, html_link* link, gui_icon_data* icon, double icon_angle)
{
    if (drawer->links == NULL) {
        drawer->links = (gui_link*)malloc(sizeof(gui_link));
    } else {
        drawer->links = (gui_link*)realloc(drawer->links, sizeof(gui_link) * (drawer->link_count + 1));
    }

    gui_link* new_link = &drawer->links[drawer->link_count];
    drawer->link_count++;

    SDL_Surface* surface;
    int text_width;
    int text_height;
    if (icon == NULL) {
        surface = TTF_RenderUTF8_Shaded(drawer->font, link->inner_text.text, drawer->link_color, drawer->bg_color);
        text_width = surface->w;
        text_height = surface->h;
    } else {
        surface = create_icon_surface(drawer, icon, true);
        text_width = icon_size(drawer);
        text_height = icon_size(drawer);
    }

    new_link->texture = SDL_CreateTextureFromSurface(drawer->renderer, surface);
    SDL_FreeSurface(surface);
    new_link->rect.x = drawer->current_x;
    new_link->rect.y = drawer->current_y;
    new_link->rect.w = text_width;
    new_link->rect.h = text_height;
    new_link->short_link = link->url.text;
    new_link->inner_text = link->inner_text.text;
    new_link->highlighted = false;
    new_link->icon_angle = icon_angle;
    new_link->icon = icon;

    drawer->current_x += text_width;
}

#define add_link_texture(drawer, link) add_clickable_texture(drawer, link, NULL, 0.0)

/**
 * Destroy old link texture and create new, highlighed one.
 * Returns true if texture actually changed.
 */
bool highlight_link_texture(gui_drawer* drawer, int link_idx)
{
    gui_link* old_link = &drawer->links[link_idx];

    if (old_link->highlighted) {
        return false;
    }

    SDL_DestroyTexture(old_link->texture);

    SDL_Surface* surface;
    if (old_link->icon == NULL) {
        surface = TTF_RenderUTF8_Shaded(drawer->font, old_link->inner_text, drawer->bg_color, drawer->link_color);
    } else {
        surface = create_icon_surface(drawer, old_link->icon, true, true);
    }

    old_link->texture = SDL_CreateTextureFromSurface(drawer->renderer, surface);
    SDL_FreeSurface(surface);

    old_link->highlighted = true;
    return true;
}

/**
 * Destroy old link texture and create new, unhighlighed one
 * Returns true if texture actually changed
 */
bool unhighlight_link_texture(gui_drawer* drawer, int link_idx)
{
    gui_link* old_link = &drawer->links[link_idx];

    if (!old_link->highlighted) {
        return false;
    }

    SDL_DestroyTexture(old_link->texture);
    SDL_Surface* surface;
    if (old_link->icon == NULL) {
        surface = TTF_RenderUTF8_Shaded(drawer->font, old_link->inner_text, drawer->link_color, drawer->bg_color);
    } else {
        surface = create_icon_surface(drawer, old_link->icon, true, false);
    }

    old_link->texture = SDL_CreateTextureFromSurface(drawer->renderer, surface);
    SDL_FreeSurface(surface);

    old_link->highlighted = false;
    return true;
}

void add_title_to_drawer(gui_drawer* drawer, html_parser* parser)
{
    if (global_config.no_title)
        return;

    // Leave some empty space to have some breathing room
    drawer->current_y = drawer->line_height;

    int links_start = calc_middle_x(drawer, TOP_NAVIGATION_STRING_LENGTH);
    if (links_start <= 10) {
        drawer->current_x = 10;
        links_start = 10;
    } else {
        drawer->current_x = links_start;
    }

    char page[5] = { 'P', 0, 0, 0, '\0' };
    memcpy(page + 1, drawer->current_page, 3);
    set_gui_text_texture(drawer, &drawer->title[0], page);

    fmt_time time = current_time();
    // Don't try to show time if there's no time to be shown or if the screen is too small
    if (time.time_len > 0 && links_start > 10) {
        // Set x so the last character is aligned with the last character of the nav links
        drawer->current_x = links_start + ((TOP_NAVIGATION_STRING_LENGTH - time.time_len) * drawer->char_width);
        set_gui_text_texture(drawer, &drawer->title[2], time.time);
    } else {
        drawer->title[2].texture = NULL;
    }

    // The actual title doesn't exsist if curl load failed
    if (!parser->curl_load_error) {
        drawer->current_x = calc_middle_x(drawer, parser->title.size);
        if (drawer->current_x <= 10 + drawer->char_width * 5) {
            drawer->current_x = 10 + drawer->char_width * 5;
        }
        set_gui_text_texture(drawer, &drawer->title[1], parser->title.text);
    }
}

void update_title_to_drawer(gui_drawer* drawer, html_parser* parser)
{
    for (int ii = 0; ii < 3; ii++) {
        if (drawer->title[ii].texture != NULL) {
            SDL_DestroyTexture(drawer->title[ii].texture);
        }
    }

    add_title_to_drawer(drawer, parser);
}

void add_top_navigation_to_drawer(gui_drawer* drawer, html_item* top_navigation)
{
    if (global_config.no_nav || global_config.no_top_nav)
        return;

    // Add nav textures
    drawer->current_y += 2 * drawer->line_height;
    drawer->current_x = calc_middle_x(drawer, TOP_NAVIGATION_STRING_LENGTH);
    bool use_icons = false;
    if (drawer->current_x <= 10) {
        int window_w, window_h;
        SDL_GetWindowSize(drawer->window, &window_w, &window_h);
        // size of icons and the " | " strings between them
        int icons_width = (icon_size(drawer) * TOP_NAVIGATION_SIZE) + (drawer->char_width * 3 * (TOP_NAVIGATION_SIZE - 1));
        drawer->current_x = window_w / 2 - icons_width / 2;
        use_icons = true;
    }

    double icon_rotates[TOP_NAVIGATION_SIZE] = { 270.0, 0.0, 180.0, 90.0 };
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        if (top_navigation[i].type == HTML_LINK) {
            if (!use_icons) {
                add_link_texture(drawer, &html_item_as_link(top_navigation[i]));
            } else {
                add_clickable_texture(drawer, &html_item_as_link(top_navigation[i]), &arrow_icon, icon_rotates[i]);
            }
        } else {
            if (!use_icons) {
                add_text_texture(drawer, html_item_as_text(top_navigation[i]).text);
            } else {
                add_text_texture(drawer, html_item_as_text(top_navigation[i]).text, &arrow_icon, icon_rotates[i]);
            }
        }

        if (i < TOP_NAVIGATION_SIZE - 1) {
            add_text_texture(drawer, " | ");
        }
    }
}

void add_middle_to_drawer(gui_drawer* drawer, html_parser* parser)
{
    if (global_config.no_middle)
        return;

    html_item_type last_type = HTML_TEXT;
    int mid_start_x = calc_middle_x(drawer, MIDDLE_TEXT_MAX_LEN);

    for (size_t i = 0; i < parser->middle_rows; i++) {
        drawer->current_x = mid_start_x;
        drawer->current_y += drawer->line_height;
        for (size_t j = 0; j < parser->middle[i].size; j++) {
            html_item* item = &parser->middle[i].items[j];
            if (item->type == HTML_LINK) {
                if (last_type == HTML_LINK) {
                    add_text_texture(drawer, "-");
                    drawer->current_x += 1;
                }

                add_link_texture(drawer, &item->item.link);
                last_type = HTML_LINK;
            } else if (item->type == HTML_TEXT) {
                add_text_texture(drawer, item->item.text.text);
                last_type = HTML_TEXT;
            }
        }
    }
}

void add_subpages_to_drawer(gui_drawer* drawer, html_parser* parser)
{
    if (global_config.no_sub_page)
        return;

    drawer->current_x = calc_middle_x(drawer, MIDDLE_TEXT_MAX_LEN);
    drawer->current_y += drawer->line_height;
    for (size_t i = 0; i < parser->sub_pages.size; i++) {
        html_item* item = &parser->sub_pages.items[i];
        if (item->type == HTML_LINK) {
            add_link_texture(drawer, &item->item.link);
        } else if (item->type == HTML_TEXT) {
            add_text_texture(drawer, item->item.text.text);
        }
    }
}

void add_bottom_navigation_to_drawer(gui_drawer* drawer, html_parser* parser)
{
    if (global_config.no_nav || global_config.no_bottom_nav)
        return;

    // first row is identical to top navigation
    add_top_navigation_to_drawer(drawer, parser->top_navigation);

    html_link* links = parser->bottom_navigation;

    // Start length with adding sizes of " | " separators
    size_t links_len = (BOTTOM_NAVIGATION_SIZE - 1) * 3;
    for (size_t i = 0; i < BOTTOM_NAVIGATION_SIZE; i++) {
        links_len += html_link_text_size(links[i]);
    }

    // Draw second navigation row
    drawer->current_y += drawer->line_height;
    drawer->current_x = calc_middle_x(drawer, links_len);
    if (drawer->current_x <= 10) {
        // TODO: support more icons/links
        int window_w, window_h;
        SDL_GetWindowSize(drawer->window, &window_w, &window_h);
        drawer->current_x = window_w / 2 - icon_size(drawer) / 2;
        add_clickable_texture(drawer, &links[BOTTOM_NAVIGATION_SIZE - 1], &home_icon, 0.0);
    } else {
        for (size_t i = 0; i < BOTTOM_NAVIGATION_SIZE; i++) {
            add_link_texture(drawer, &links[i]);
            if (i < BOTTOM_NAVIGATION_SIZE - 1) {
                add_text_texture(drawer, " | ");
            }
        }
    }
}

void render_drawer_texts(gui_drawer* drawer)
{
    // If there's error to be shown, show it, page and time
    if (drawer->error.texture != NULL) {
        SDL_RenderCopy(drawer->renderer, drawer->title[0].texture, NULL, &drawer->title[0].rect);
        SDL_RenderCopy(drawer->renderer, drawer->title[2].texture, NULL, &drawer->title[2].rect);
        SDL_RenderCopy(drawer->renderer, drawer->error.texture, NULL, &drawer->error.rect);
        return;
    }

    for (int ii = 0; ii < drawer->text_count; ii++) {
        gui_text text = drawer->texts[ii];
        if (text.icon_angle != 0.0) {
            SDL_RenderCopyEx(drawer->renderer, text.texture, NULL, &text.rect, text.icon_angle, NULL, SDL_FLIP_NONE);
        } else {
            SDL_RenderCopy(drawer->renderer, text.texture, NULL, &text.rect);
        }
    }

    for (int ii = 0; ii < drawer->link_count; ii++) {
        gui_link link = drawer->links[ii];
        if (link.icon_angle != 0.0) {
            SDL_RenderCopyEx(drawer->renderer, link.texture, NULL, &link.rect, link.icon_angle, NULL, SDL_FLIP_NONE);
        } else {
            SDL_RenderCopy(drawer->renderer, link.texture, NULL, &link.rect);
        }
    }

    for (int ii = 0; ii < 3; ii++) {
        gui_text title = drawer->title[ii];
        if (title.texture != NULL) {
            SDL_RenderCopy(drawer->renderer, title.texture, NULL, &title.rect);
        }
    }
}

/**
 * returns true if mouse hover requires a rerender
 */
bool check_mouse_hover(gui_drawer* drawer, bool mouse_on_imgui)
{
    bool rerender = false;

    // If mouse is caputured by imgui, we want to reset all the higlights
    if (mouse_on_imgui) {
        for (int ii = 0; ii < drawer->link_count; ii++) {
            if (unhighlight_link_texture(drawer, ii)) {
                rerender = true;
            }
        }

        return rerender;
    }

    int mx, my;
    SDL_GetMouseState(&mx, &my);
    for (int ii = 0; ii < drawer->link_count; ii++) {
        gui_link link = drawer->links[ii];
        int lx = link.rect.x;
        int ly = link.rect.y;
        int lw = link.rect.w;
        int lh = link.rect.h;

        // is mouse on top of the link rectangle
        if (mx >= lx && mx <= lx + lw && my >= ly && my <= ly + lh) {
            if (highlight_link_texture(drawer, ii)) {
                rerender = true;
            }
        } else {
            if (unhighlight_link_texture(drawer, ii)) {
                rerender = true;
            }
        }
    }

    return rerender;
}

/**
 * Get width and height of a font.
 * This requires a monospace font or the size of the font will not give
 * us accurate padding
 */
void get_font_dimensions(gui_drawer* drawer)
{
    SDL_Color tmpColor = { 0, 0, 0, 0 };
    // Create a single char temporarily to calculate our dimensions
    SDL_Surface* surface = TTF_RenderUTF8_Solid(drawer->font, "A", tmpColor);
    drawer->char_width = surface->w;
    drawer->line_height = surface->h;
    SDL_FreeSurface(surface);
}

void set_render_texts(gui_drawer* drawer, html_parser* parser)
{
    // Just to be sure
    free_render_texts(drawer);
    // TODO: update config in realtime. now this function breaks imgui settigns editing
    // set_config(drawer);
    get_font_dimensions(drawer);
    add_title_to_drawer(drawer, parser);

    if (parser->curl_load_error) {
        add_error_message(drawer, "Couldn't load the page. Try again.");
        return;
    }

    add_top_navigation_to_drawer(drawer, parser->top_navigation);
    add_middle_to_drawer(drawer, parser);
    add_subpages_to_drawer(drawer, parser);
    add_bottom_navigation_to_drawer(drawer, parser);
}

bool imgui_color_edit_line(const char* text, ImVec4* source, SDL_Color* target)
{
    if (ImGui::ColorEdit3(text, (float*)source)) {
        target->r = source->x * 255;
        target->g = source->y * 255;
        target->b = source->z * 255;
        return true;
    }

    return false;
}

bool imgui_settings_window(gui_drawer* drawer)
{
#define edit(fn)       \
    if (fn) {          \
        edited = true; \
    }

#define font_edit(fn)     \
    if (fn) {             \
        set_font(drawer); \
        edited = true;    \
    }

    bool edited = false;
    bool* show_window = &drawer->imgui.show_settings;
    if (ImGui::Begin("Background color setting", show_window)) {
        edit(imgui_color_edit_line("Link color", &drawer->imgui.elink_color, &drawer->link_color));
        edit(imgui_color_edit_line("Text color", &drawer->imgui.etext_color, &drawer->text_color));
        edit(imgui_color_edit_line("Background color", &drawer->imgui.ebg_color, &drawer->bg_color));
        ImGui::Separator();
        font_edit(ImGui::DragInt("Font size", &drawer->font_size, 1, 8, 80));
        ImGui::Separator();
        ImGui::Checkbox("Auto refresh", &drawer->auto_refresh);
        if (drawer->auto_refresh) {
            if (ImGui::DragInt("Auto refresh timer", &drawer->refresh_interval, 1, 10, 99999)) {
                drawer->refresh_timer = 0;
            }
        }
    }
    ImGui::End();
    return edited;

#undef edit
#undef font_edit
}

bool imgui_menu_bar(gui_drawer* drawer)
{
    bool open_window = false;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Settings", NULL, drawer->imgui.show_settings)) {
                drawer->imgui.show_settings = !drawer->imgui.show_settings;
                open_window = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Controls", NULL, false, false)) {
            }
            ImGui::Separator();
            if (ImGui::MenuItem("About", NULL, false, false)) {
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    return open_window;
}

/**
 * Load a new page.
 * Note that the new page has to be set in the parser before calling this
 */
void load_new_page(gui_drawer* drawer, html_parser* parser, bool add_history)
{
    // we don't currenlty support sub pages
    // TODO: support sub pages
    bool new_page = memcmp(drawer->current_page, parser->link, 3) != 0;

    // Copy the new page to the current page info
    memcpy(drawer->current_page, parser->link, 3);
    free_html_parser(parser);
    init_html_parser(parser);
    load_page(parser);
    parse_html(parser);
    set_render_texts(drawer, parser);

    if (!parser->curl_load_error && new_page && add_history) {
        add_history_link(parser->link);
    }
}

void init_imgui(gui_drawer* drawer)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(drawer->window, drawer->renderer);
    ImGui_ImplSDLRenderer_Init(drawer->renderer);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = get_imgui_ini_path();
}

int display_gui(html_parser* parser)
{
// Hide the terminal before opening the GUI on windows
#ifdef _WIN32
    HWND windowHandle = GetConsoleWindow();
    ShowWindow(windowHandle, SW_HIDE);
#endif

    gui_drawer main_drawer;
    SDL_Event event;
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    // init drawers
    init_gui_drawer(&main_drawer);
    TTF_Init();
    init_imgui(&main_drawer);

    set_font(&main_drawer);
    set_render_texts(&main_drawer, parser);

    if (!parser->curl_load_error) {
        add_history_link(parser->link);
    }

    ImGuiIO& io = ImGui::GetIO();

    int quit = 0;
    int redraw_timer = 0;
    int input_idx = 0;
    bool redraw = true;
    while (!quit) {
        while (SDL_PollEvent(&event) == 1) {
            // If imgui uses the event, don't pass it to the teletext gui
            if (ImGui_ImplSDL2_ProcessEvent(&event)) {
                // TODO: separate render events for imgui and teletext renders?
                redraw = true;
            }

            if (event.type == SDL_QUIT) {
                quit = 1;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    set_render_texts(&main_drawer, parser);
                    redraw = true;
                }
            } else if (!io.WantCaptureMouse && event.type == SDL_MOUSEBUTTONUP) {
                if (check_mouse_click(&main_drawer, parser, &event)) {
                    redraw = true;
                    input_idx = 0;
                }
            } else if (!io.WantCaptureKeyboard && event.type == SDL_KEYUP) {
                int sym = event.key.keysym.sym;
                if (sym >= SDLK_0 && sym <= SDLK_9) {
                    main_drawer.current_page[input_idx] = (char)sym;
                    input_idx++;
                    if (input_idx >= 3) {
                        input_idx = 0;
                        // TODO: error if num == -1
                        int num = page_number(main_drawer.current_page);
                        link_from_ints(parser, num, 1);
                        load_new_page(&main_drawer, parser, true);
                    } else {
                        for (int idx = input_idx; idx < 3; idx++) {
                            main_drawer.current_page[idx] = '-';
                        }
                        update_title_to_drawer(&main_drawer, parser);
                    }

                    redraw = true;
                }
            }
        }

        if (check_mouse_hover(&main_drawer, io.WantCaptureMouse)) {
            redraw = true;
        }

        // redraw every 1000 milliseconds (1 second) to update the time
        if (redraw_timer >= 1000) {
            redraw_timer = 0;
            redraw = true;
            update_title_to_drawer(&main_drawer, parser);
        }

        // refresh_interval is in seconds, refresh timer is in milliseconds
        if (main_drawer.refresh_timer >= main_drawer.refresh_interval * 1000) {
            main_drawer.refresh_timer = 0;
            if (main_drawer.auto_refresh) {
                load_new_page(&main_drawer, parser, false);
                redraw = true;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (imgui_menu_bar(&main_drawer)) {
            redraw = true;
        }

        if (main_drawer.imgui.show_settings) {
            if (imgui_settings_window(&main_drawer)) {
                set_render_texts(&main_drawer, parser);
                redraw = true;
            }
        }

        ImGui::Render();
        if (redraw) {
            Uint8 r = main_drawer.bg_color.r;
            Uint8 g = main_drawer.bg_color.g;
            Uint8 b = main_drawer.bg_color.b;
            SDL_SetRenderDrawColor(main_drawer.renderer, r, g, b, 0);
            SDL_RenderClear(main_drawer.renderer);
            render_drawer_texts(&main_drawer);
            ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
            SDL_RenderPresent(main_drawer.renderer);
        }

        redraw = false;
        SDL_Delay(REDRW_DELAY);
        redraw_timer += REDRW_DELAY;
        main_drawer.refresh_timer += REDRW_DELAY;
    }

    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    set_gui_options_to_be_saved(&main_drawer);
    free_gui_drawer(&main_drawer);

    // Remember to destroy SDL stuff to avoid memleaks
    if (TTF_WasInit()) {
        TTF_CloseFont(main_drawer.font);
        TTF_Quit();
    }

    if (SDL_WasInit(0)) {
        SDL_Quit();
    }

    save_config();

    return 0;
}
