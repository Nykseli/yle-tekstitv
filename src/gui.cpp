#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_ttf.h>
#include <imgui.h>
#include <imgui/backends/imgui_impl_sdl.h>
#include <imgui/backends/imgui_impl_sdlrenderer.h>

#include "gui.h"

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

    char current_page[4];

    // settings for imgui specific things
    imgui_settings imgui;
} gui_drawer;

#define WINDOW_WIDTH 760
#define WINDOW_HEIGHT 800
// milliseconds between redraws
// 16ms is roughly 60 fps
#define REDRW_DELAY 16

// TODO: how to bundle up the font to the binary?
const char* font_path = "assets/test_font.ttf";

ImVec4 sdl_color_to_imvec4(SDL_Color* src)
{

    Uint8 r = src->r;
    Uint8 g = src->g;
    Uint8 b = src->b;
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.00f);
}

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

    drawer->imgui.ebg_color = sdl_color_to_imvec4(&drawer->bg_color);
    drawer->imgui.etext_color = sdl_color_to_imvec4(&drawer->text_color);
    drawer->imgui.elink_color = sdl_color_to_imvec4(&drawer->link_color);
    drawer->imgui.show_settings = false;
    // TODO: get these from settings
    drawer->font_size = 16;
    drawer->w_width = WINDOW_WIDTH;
    drawer->w_height = WINDOW_HEIGHT;
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

int set_gui_text_texture(gui_drawer* drawer, gui_text* new_text, const char* text)
{
    SDL_Surface* surface = TTF_RenderUTF8_Solid(drawer->font, text, drawer->text_color);
    new_text->texture = SDL_CreateTextureFromSurface(drawer->renderer, surface);
    int text_width = surface->w;
    int text_height = surface->h;
    SDL_FreeSurface(surface);
    new_text->rect.x = drawer->current_x;
    new_text->rect.y = drawer->current_y;
    new_text->rect.w = text_width;
    new_text->rect.h = text_height;

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
void add_text_texture(gui_drawer* drawer, const char* text)
{
    if (drawer->texts == NULL) {
        drawer->texts = (gui_text*)malloc(sizeof(gui_text));
    } else {
        drawer->texts = (gui_text*)realloc(drawer->texts, sizeof(gui_text) * (drawer->text_count + 1));
    }

    gui_text* new_text = &drawer->texts[drawer->text_count];
    drawer->text_count++;

    drawer->current_x += set_gui_text_texture(drawer, new_text, text);
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
    SDL_Surface* surface = TTF_RenderUTF8_Shaded(drawer->font, old_link->inner_text, drawer->bg_color, drawer->link_color);
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
    SDL_Surface* surface = TTF_RenderUTF8_Solid(drawer->font, old_link->inner_text, drawer->link_color);
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
    drawer->current_x = links_start;
    char page[5] = { 'P', 0, 0, 0, '\0' };
    memcpy(page + 1, drawer->current_page, 3);
    set_gui_text_texture(drawer, &drawer->title[0], page);

    fmt_time time = current_time();
    // Don't try to show time if there's no time to be shown
    if (time.time_len > 0) {
        // Set x so the last character is aligned with the last character of the nav links
        drawer->current_x = links_start + ((TOP_NAVIGATION_STRING_LENGTH - time.time_len) * drawer->char_width);
        set_gui_text_texture(drawer, &drawer->title[2], time.time);
    } else {
        drawer->title[2].texture = NULL;
    }

    // The actual title doesn't exsist if curl load failed
    if (!parser->curl_load_error) {
        drawer->current_x = calc_middle_x(drawer, parser->title.size);
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
    // If there's error to be shown, show it, page and time
    if (drawer->error.texture != NULL) {
        SDL_RenderCopy(drawer->renderer, drawer->title[0].texture, NULL, &drawer->title[0].rect);
        SDL_RenderCopy(drawer->renderer, drawer->title[2].texture, NULL, &drawer->title[2].rect);
        SDL_RenderCopy(drawer->renderer, drawer->error.texture, NULL, &drawer->error.rect);
        return;
    }

    for (int ii = 0; ii < drawer->text_count; ii++) {
        gui_text text = drawer->texts[ii];
        SDL_RenderCopy(drawer->renderer, text.texture, NULL, &text.rect);
    }

    for (int ii = 0; ii < drawer->link_count; ii++) {
        gui_link link = drawer->links[ii];
        SDL_RenderCopy(drawer->renderer, link.texture, NULL, &link.rect);
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
void load_new_page(gui_drawer* drawer, html_parser* parser)
{
    // Copy the new page to the current page info
    memcpy(drawer->current_page, parser->link, 3);
    free_html_parser(parser);
    init_html_parser(parser);
    load_page(parser);
    parse_html(parser);
    set_render_texts(drawer, parser);
}

void init_imgui(gui_drawer* drawer)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(drawer->window, drawer->renderer);
    ImGui_ImplSDLRenderer_Init(drawer->renderer);
}

int display_gui(html_parser* parser)
{
    gui_drawer main_drawer;
    SDL_Event event;
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    // init drawers
    init_gui_drawer(&main_drawer);
    // window settings need to be set after the creation
    SDL_SetWindowResizable(main_drawer.window, SDL_TRUE);
    TTF_Init();
    init_imgui(&main_drawer);

    set_font(&main_drawer);
    set_render_texts(&main_drawer, parser);

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
                const char* link = check_link_click(&main_drawer);
                if (link != NULL) {
                    link_from_short_link(parser, (char*)link);
                    load_new_page(&main_drawer, parser);
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
                        load_new_page(&main_drawer, parser);
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
    }

    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
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
