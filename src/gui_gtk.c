#ifdef GUI_GTK_ENABLED

#include "gui_gtk.h"

static gboolean hovering_over_link = FALSE;
static GdkCursor* hand_cursor = NULL;
static GdkCursor* regular_cursor = NULL;
static html_parser* global_parser = NULL;

// Data key for g_object_set_data
#define URL_DATA_KEY "url"
#define PAGE_LINK_MAX 32

// TODO: figure a way to store the links as pointers with
//       g_object_set_data in insert_link
typedef struct {
    gchar page_links[PAGE_LINK_MAX][HTML_LINK_SIZE + 1];
    gint size;
} page_link_list;

page_link_list page_links = {
    .size = 0
};

/**
 * Inserts a piece of text into the buffer, giving it the usual
 * appearance of a hyperlink in a web browser: blue and underlined.
 * Additionally, attaches the index of the links on the page_list,
 * to make it recognizable as a link.
 */
static void insert_link(GtkTextBuffer* buffer, GtkTextIter* iter, html_link link)
{
    // TODO: tag color based on the user config
    gchar* text = (gchar*)link.inner_text.text;
    GtkTextTag* tag = gtk_text_buffer_create_tag(buffer, NULL, "foreground", "blue", "underline", PANGO_UNDERLINE_SINGLE, NULL);
    gint page_link_index = page_links.size;
    if (page_link_index + 1 >= PAGE_LINK_MAX) {
        fprintf(stderr, "Too many links on the page. Max amount is %d\n", PAGE_LINK_MAX);
        exit(1);
    }
    strcpy(page_links.page_links[page_link_index], link.url.text);
    // Add 1 because the gtk is treats 0 as null
    g_object_set_data(G_OBJECT(tag), URL_DATA_KEY, GINT_TO_POINTER(page_link_index + 1));
    gtk_text_buffer_insert_with_tags(buffer, iter, text, -1, tag, NULL);
    page_links.size += 1;
}

/**
 * Fill the buffer with text and links from global_parser->middle
 */
static void show_page_middle(GtkTextBuffer* buffer)
{
    GtkTextIter iter;
    gtk_text_buffer_set_text(buffer, "", 0);
    gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);
    html_item_type last_type = HTML_TEXT;

    for (size_t i = 0; i < global_parser->middle_rows; i++) {
        for (size_t j = 0; j < global_parser->middle[i].size; j++) {
            html_item item = global_parser->middle[i].items[j];
            if (item.type == HTML_LINK) {
                if (last_type == HTML_LINK)
                    gtk_text_buffer_insert(buffer, &iter, "-", 1);

                insert_link(buffer, &iter, html_item_as_link(item));
                last_type = HTML_LINK;
            } else if (item.type == HTML_TEXT) {
                gtk_text_buffer_insert(buffer, &iter, html_text_text(html_item_as_text(item)), -1);
                last_type = HTML_TEXT;
            }
        }
        gtk_text_buffer_insert(buffer, &iter, "\n", 1);
    }
    gtk_text_buffer_insert(buffer, &iter, "\n", 1);
}

/**
 * Fills the buffer with text and links.
 */
static void show_page(GtkTextBuffer* buffer, gint page)
{
    // TODO: Draw all the things
    show_page_middle(buffer);
}

/**
 * Looks at all tags covering the position (x, y) in the text view,
 * and if one of them is a link, change the cursor to the "hands" cursor
 * typically used by web browsers.
 */
static void set_cursor_if_appropriate(GtkTextView* text_view, gint x, gint y)
{
    GSList *tags = NULL, *tagp = NULL;
    GtkTextIter iter;
    gboolean hovering = FALSE;

    if (gtk_text_view_get_iter_at_location(text_view, &iter, x, y)) {
        tags = gtk_text_iter_get_tags(&iter);
        for (tagp = tags; tagp != NULL; tagp = tagp->next) {
            GtkTextTag* tag = tagp->data;
            gpointer url = g_object_get_data(G_OBJECT(tag), URL_DATA_KEY);
            printf("%p\n", url);
            if (url != NULL) {
                hovering = TRUE;
                break;
            }
        }
    }

    if (hovering != hovering_over_link) {
        hovering_over_link = hovering;

        if (hovering_over_link)
            gdk_window_set_cursor(gtk_text_view_get_window(text_view, GTK_TEXT_WINDOW_TEXT), hand_cursor);
        else
            gdk_window_set_cursor(gtk_text_view_get_window(text_view, GTK_TEXT_WINDOW_TEXT), regular_cursor);
    }

    if (tags)
        g_slist_free(tags);
}

/**
 * Update the cursor image if the pointer moved.
 */
static gboolean motion_notify_event(GtkWidget* text_view, GdkEventMotion* event)
{
    gint x, y;

    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(text_view), GTK_TEXT_WINDOW_WIDGET, event->x, event->y, &x, &y);
    set_cursor_if_appropriate(GTK_TEXT_VIEW(text_view), x, y);
    return FALSE;
}

/**
 * Looks at all tags covering the position of iter in the text view,
 * and if one of them is a link, follow it by showing the page identified
 * by the data attached to it.
 */
static void follow_if_link(GtkWidget* text_view, GtkTextIter* iter)
{
    GSList* tagp = NULL;
    GSList* tags = gtk_text_iter_get_tags(iter);
    for (tagp = tags; tagp != NULL; tagp = tagp->next) {
        GtkTextTag* tag = tags->data;
        gpointer url_pointer = g_object_get_data(G_OBJECT(tag), URL_DATA_KEY);

        if (url_pointer != 0) {
            // -1 Because we had to add 1 because gtk. See insert_link function
            gint url_index = GPOINTER_TO_INT(url_pointer) - 1;
            printf("Found at: %d. url: %s\n", url_index, page_links.page_links[url_index]);
            break;
        }
    }

    if (tags)
        g_slist_free(tags);
}

/**
 * Links can also be activated by clicking or tapping.
 * Called when the click/tap is released
 */
static gboolean click_event_after(GtkWidget* text_view, GdkEvent* ev)
{
    GtkTextIter start, end, iter;
    GtkTextBuffer* buffer;
    gdouble ex, ey;
    gint x, y;

    if (ev->type == GDK_BUTTON_RELEASE) {
        GdkEventButton* event = (GdkEventButton*)ev;
        if (event->button != GDK_BUTTON_PRIMARY)
            return FALSE;

        ex = event->x;
        ey = event->y;
    } else if (ev->type == GDK_TOUCH_END) {
        GdkEventTouch* event = (GdkEventTouch*)ev;

        ex = event->x;
        ey = event->y;
    } else
        return FALSE;

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    /* we shouldn't follow a link if the user has selected something */
    gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
    if (gtk_text_iter_get_offset(&start) != gtk_text_iter_get_offset(&end))
        return FALSE;

    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(text_view), GTK_TEXT_WINDOW_WIDGET, ex, ey, &x, &y);

    if (gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view), &iter, x, y))
        follow_if_link(text_view, &iter);

    return TRUE;
}

static void activate(GtkApplication* app, gpointer user_data)
{
    GtkWidget* window;
    // Init the base window
    // TODO: create a window inside the main window and center the inner window
    window = gtk_application_window_new(app);
    // Cursor for hovering over the links
    GdkDisplay* display = gtk_widget_get_display(window);
    hand_cursor = gdk_cursor_new_from_name(display, "pointer");
    regular_cursor = gdk_cursor_new_from_name(display, "text");
    gtk_window_set_title(GTK_WINDOW(window), "Teksti-TV");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 400);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    GtkWidget* text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view), 20);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view), 20);

    // TODO: keyboard support for navigating the links
    g_signal_connect(text_view, "event-after", G_CALLBACK(click_event_after), NULL);
    g_signal_connect(text_view, "motion-notify-event", G_CALLBACK(motion_notify_event), NULL);

    GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(text_view));
    show_page(text_buffer, 1);

    gtk_widget_show_all(window);
}

int open_gtk_gui(html_parser* parser)
{
    global_parser = parser;
    GtkApplication* app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);

    return status;
}

#endif // GUI_GTK_ENABLED
