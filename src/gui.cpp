#include <stdlib.h>

#include "gui.h"

#define WINDOW_WIDTH 760
#define WINDOW_HEIGHT 800

void init_gui_drawer(gui_drawer* drawer)
{
    // Initialise the structure
    drawer->window = NULL;
    drawer->renderer = NULL;
    drawer->texts = NULL;
    drawer->font = NULL;
    drawer->char_width = 0;
    drawer->line_height = 0;
    drawer->text_count = 0;
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

    drawer->char_width = 0;
    drawer->line_height = 0;
    drawer->text_count = 0;
    drawer->w_width = 0;
    drawer->w_height = 0;
    drawer->current_x = 0;
    drawer->current_y = 0;
    drawer->texts = NULL;
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

/**
 * Add a new text texture, texture will be added to the current rectangle
 * of the drawer and the current x pos is updated to the end of the text
 */
void add_text_texture(gui_drawer* drawer, const char* text)
{
    SDL_Color textColor = { 255, 255, 255, 0 };
    if (drawer->texts == NULL) {
        drawer->texts = (gui_text*)malloc(sizeof(gui_text));
    } else {
        drawer->texts = (gui_text*)realloc(drawer->texts, sizeof(gui_text) * (drawer->text_count + 1));
    }

    gui_text* new_text = &drawer->texts[drawer->text_count];
    drawer->text_count++;

    SDL_Surface* surface = TTF_RenderUTF8_Solid(drawer->font, text, textColor);
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

void add_title_to_drawer(gui_drawer* drawer, html_text title)
{
    // Leave some empty space to have some breathing room
    drawer->current_y = drawer->line_height;
    drawer->current_x = calc_middle_x(drawer, title.size);
    add_text_texture(drawer, title.text);
}

void add_top_navigation_to_drawer(gui_drawer* drawer, html_item* top_navigation)
{
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
            // TODO: add link texture
            add_text_texture(drawer, html_item_as_link(top_navigation[i]).inner_text.text);
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
    html_item_type last_type = HTML_TEXT;
    int mid_start_x = calc_middle_x(drawer, MIDDLE_TEXT_MAX_LEN);

    for (size_t i = 0; i < parser->middle_rows; i++) {
        drawer->current_x = mid_start_x;
        drawer->current_y += drawer->line_height;
        for (size_t j = 0; j < parser->middle[i].size; j++) {
            html_item item = parser->middle[i].items[j];
            if (item.type == HTML_LINK) {
                if (last_type == HTML_LINK) {
                    add_text_texture(drawer, "-");
                    drawer->current_x += 1;
                }

                // TODO: add link texture
                add_text_texture(drawer, html_item_as_link(item).inner_text.text);
                last_type = HTML_LINK;
            } else if (item.type == HTML_TEXT) {
                add_text_texture(drawer, html_item_as_text(item).text);
                last_type = HTML_TEXT;
            }
        }
    }
}

void add_subpages_to_drawer(gui_drawer* drawer, html_parser* parser)
{
    drawer->current_x = calc_middle_x(drawer, MIDDLE_TEXT_MAX_LEN);
    drawer->current_y += drawer->line_height;
    for (size_t i = 0; i < parser->sub_pages.size; i++) {
        html_item item = parser->sub_pages.items[i];
        if (item.type == HTML_LINK) {
            // TODO: add link texture
            add_text_texture(drawer, html_item_as_link(item).inner_text.text);
        } else if (item.type == HTML_TEXT) {
            add_text_texture(drawer, html_item_as_text(item).text);
        }
    }
}

void add_bottom_navigation_to_drawer(gui_drawer* drawer, html_parser* parser)
{
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
        // TODO: add link texture
        add_text_texture(drawer, links[i].inner_text.text);

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
            }
        }

        SDL_SetRenderDrawColor(main_drawer.renderer, 0, 0, 0, 0);
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
