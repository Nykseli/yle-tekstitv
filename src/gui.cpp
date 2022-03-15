#include <stdlib.h>

#include "gui.h"

#define WINDOW_WIDTH 760
#define WINDOW_HEIGHT 800

SDL_Color config_rgb_to_color(short* rgb, SDL_Color def)
{
#define SHORT_8BIT(s) (Uint8)(255 * (s / 1000.0f))

    if (*rgb == -1 || global_config.default_colors) {
        return def;
    }

    Uint8 r = SHORT_8BIT(rgb[0]);
    Uint8 g = SHORT_8BIT(rgb[1]);
    Uint8 b = SHORT_8BIT(rgb[2]);

    SDL_Color color = { r, g, b, 0 };
    return color;

#undef SHORT_8BIT
}

void set_config(gui_drawer* drawer)
{
    SDL_Color bgColor = { 0, 0, 0, 0 };
    SDL_Color textColor = { 255, 255, 255, 0 };
    SDL_Color linkColor = { 17, 159, 244, 0 };

    drawer->bg_color = config_rgb_to_color(global_config.bg_rgb, bgColor);
    drawer->text_color = config_rgb_to_color(global_config.text_rgb, textColor);
    drawer->link_color = config_rgb_to_color(global_config.link_rgb, linkColor);
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

    // Initialize SDL
    SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &drawer->window, &drawer->renderer);
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

    drawer->char_width = 0;
    drawer->line_height = 0;
    drawer->text_count = 0;
    drawer->link_count = 0;
    drawer->w_width = 0;
    drawer->w_height = 0;
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

/**
 * Add a new text texture, texture will be added to the current rectangle
 * of the drawer and the current x pos is updated to the end of the text
 */
void add_text_texture(gui_drawer* drawer, const char* text)
{
    if (drawer->texts == NULL) {
        drawer->texts = (gui_text*)malloc(sizeof(gui_text));
    } else {
        drawer->texts = (gui_text*)realloc(drawer->texts, sizeof(gui_text) * (drawer->text_count + 1));
    }

    gui_text* new_text = &drawer->texts[drawer->text_count];
    drawer->text_count++;

    SDL_Surface* surface = TTF_RenderUTF8_Solid(drawer->font, text, drawer->text_color);
    new_text->texture = SDL_CreateTextureFromSurface(drawer->renderer, surface);
    int text_width = surface->w;
    int text_height = surface->h;
    SDL_FreeSurface(surface);
    new_text->rect.x = drawer->current_x;
    new_text->rect.y = drawer->current_y;
    new_text->rect.w = text_width;
    new_text->rect.h = text_height;

    drawer->current_x += text_width;
}

/**
 * Add a new text texture, texture will be added to the current rectangle
 * of the drawer and the current x pos is updated to the end of the text
 */
void add_link_texture(gui_drawer* drawer, html_link* link)
{
    if (drawer->links == NULL) {
        drawer->links = (gui_link*)malloc(sizeof(gui_link));
    } else {
        drawer->links = (gui_link*)realloc(drawer->links, sizeof(gui_link) * (drawer->link_count + 1));
    }

    gui_link* new_link = &drawer->links[drawer->link_count];
    drawer->link_count++;

    SDL_Surface* surface = TTF_RenderUTF8_Solid(drawer->font, link->inner_text.text, drawer->link_color);
    new_link->texture = SDL_CreateTextureFromSurface(drawer->renderer, surface);
    int text_width = surface->w;
    int text_height = surface->h;
    SDL_FreeSurface(surface);
    new_link->rect.x = drawer->current_x;
    new_link->rect.y = drawer->current_y;
    new_link->rect.w = text_width;
    new_link->rect.h = text_height;
    new_link->short_link = link->url.text;
    new_link->inner_text = link->inner_text.text;
    new_link->highlighted = false;

    drawer->current_x += text_width;
}

/**
 * Destroy old link texture and create new, highlighed one
 */
void highlight_link_texture(gui_drawer* drawer, int link_idx)
{
    gui_link* old_link = &drawer->links[link_idx];

    if (old_link->highlighted) {
        return;
    }

    SDL_DestroyTexture(old_link->texture);
    SDL_Surface* surface = TTF_RenderUTF8_Shaded(drawer->font, old_link->inner_text, drawer->bg_color, drawer->link_color);
    old_link->texture = SDL_CreateTextureFromSurface(drawer->renderer, surface);
    SDL_FreeSurface(surface);

    old_link->highlighted = true;
}

/**
 * Destroy old link texture and create new, unhighlighed one
 */
void unhighlight_link_texture(gui_drawer* drawer, int link_idx)
{
    gui_link* old_link = &drawer->links[link_idx];

    if (!old_link->highlighted) {
        return;
    }

    SDL_DestroyTexture(old_link->texture);
    SDL_Surface* surface = TTF_RenderUTF8_Solid(drawer->font, old_link->inner_text, drawer->link_color);
    old_link->texture = SDL_CreateTextureFromSurface(drawer->renderer, surface);
    SDL_FreeSurface(surface);

    old_link->highlighted = false;
}

void add_title_to_drawer(gui_drawer* drawer, html_text title)
{
    if (global_config.no_title)
        return;

    // Leave some empty space to have some breathing room
    drawer->current_y = drawer->line_height;
    drawer->current_x = calc_middle_x(drawer, title.size);
    add_text_texture(drawer, title.text);
}

void add_top_navigation_to_drawer(gui_drawer* drawer, html_item* top_navigation)
{
    if (global_config.no_nav || global_config.no_top_nav)
        return;

    // Start length with adding sizes of " | " separators
    size_t links_len = (TOP_NAVIGATION_SIZE - 1) * 3;
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        links_len += html_item_text_size(top_navigation[i]);
    }

    // Add nav textures
    drawer->current_y += 2 * drawer->line_height;
    drawer->current_x = calc_middle_x(drawer, links_len);
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        if (top_navigation[i].type == HTML_LINK) {
            add_link_texture(drawer, &html_item_as_link(top_navigation[i]));
        } else {
            add_text_texture(drawer, html_item_as_text(top_navigation[i]).text);
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
    for (size_t i = 0; i < BOTTOM_NAVIGATION_SIZE; i++) {
        add_link_texture(drawer, &links[i]);

        if (i < BOTTOM_NAVIGATION_SIZE - 1) {
            add_text_texture(drawer, " | ");
        }
    }
}

void render_drawer_texts(gui_drawer* drawer)
{
    for (int ii = 0; ii < drawer->text_count; ii++) {
        gui_text text = drawer->texts[ii];
        SDL_RenderCopy(drawer->renderer, text.texture, NULL, &text.rect);
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
            highlight_link_texture(drawer, ii);
        } else {
            unhighlight_link_texture(drawer, ii);
        }

        SDL_RenderCopy(drawer->renderer, link.texture, NULL, &link.rect);
    }
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
    // update config in realtime
    set_config(drawer);
    get_font_dimensions(drawer);
    add_title_to_drawer(drawer, parser->title);
    add_top_navigation_to_drawer(drawer, parser->top_navigation);
    add_middle_to_drawer(drawer, parser);
    add_subpages_to_drawer(drawer, parser);
    add_bottom_navigation_to_drawer(drawer, parser);
}

int display_gui(html_parser* parser)
{
    // TODO: how to bundle up the font to the binary?
    const char* font_path = "assets/test_font.ttf";

    gui_drawer main_drawer;
    SDL_Event event;
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);
    // init drawers
    init_gui_drawer(&main_drawer);
    // window settings need to be set after the creation
    SDL_SetWindowResizable(main_drawer.window, SDL_TRUE);
    TTF_Init();

    main_drawer.font = TTF_OpenFont(font_path, 16);
    if (main_drawer.font == NULL) {
        fprintf(stderr, "error: font not found\n");
        exit(EXIT_FAILURE);
    }

    set_render_texts(&main_drawer, parser);

    int quit = 0;
    while (!quit) {
        while (SDL_PollEvent(&event) == 1) {
            if (event.type == SDL_QUIT) {
                quit = 1;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    set_render_texts(&main_drawer, parser);
                }
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                const char* link = check_link_click(&main_drawer);
                if (link != NULL) {
                    link_from_short_link(parser, (char*)link);
                    free_html_parser(parser);
                    init_html_parser(parser);
                    load_page(parser);
                    parse_html(parser);
                    set_render_texts(&main_drawer, parser);
                }
            }
        }

        // TODO: only rerender when something has changed
        Uint8 r = main_drawer.bg_color.r;
        Uint8 g = main_drawer.bg_color.g;
        Uint8 b = main_drawer.bg_color.b;
        SDL_SetRenderDrawColor(main_drawer.renderer, r, g, b, 0);
        SDL_RenderClear(main_drawer.renderer);
        render_drawer_texts(&main_drawer);
        SDL_RenderPresent(main_drawer.renderer);
        // 40ms is 25 fps
        SDL_Delay(40);
    }

    free_gui_drawer(&main_drawer);

    // Remember to destroy SDL stuff to avoid memleaks
    if (TTF_WasInit()) {
        TTF_CloseFont(main_drawer.font);
        TTF_Quit();
    }

    if (SDL_WasInit(0)) {
        SDL_Quit();
    }

    return 0;
}
