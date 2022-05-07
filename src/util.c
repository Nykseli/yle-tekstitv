
#include <stdio.h>
#include <string.h>
#include <tekstitv.h>

#define HISTORY_CAPACITY 16

#define HISTORY_NEXT(val) (((val) + 1) % HISTORY_CAPACITY)
#define HISTORY_PREV(val) (((val) + HISTORY_CAPACITY - 1) % HISTORY_CAPACITY)
#define HISTORY_CURRENT_LINK (history.entries[history.current])

typedef struct {
    // Storage for browser links
    char entries[HISTORY_CAPACITY][HTML_LINK_SIZE + 1];
    int count; // Amount of items in the buffer
    int start; // Start of the valid data
    int current; // Index of the value we are currently using
} browser_history;

browser_history history = {
    .count = 0,
    .start = 0,
    .current = -1,
};

static bool history_at_last_link()
{
    if (history.start == 0) {
        if (history.current == history.count - 1)
            return true;
    } else if (history.current == history.start - 1) {
        return true;
    }

    return false;
}

void add_history_link(char* link)
{
    bool at_last = history_at_last_link();

    // If history is not full and we are at the last link we can just add new link
    if (history.count != HISTORY_CAPACITY && at_last) {
        history.count++;
        history.current = HISTORY_NEXT(history.current);
        memcpy(history.entries[history.current], link, HTML_LINK_SIZE);
        history.entries[history.current][HTML_LINK_SIZE] = '\0';
        return;
    }

    // If not at the last link, "Override" the links after current
    if (!at_last) {
        // + 2 Because there will be atleast the current with the upcoming link
        if (history.start == 0 || history.start < history.current) {
            history.count = history.current - history.start + 2;
        } else {
            history.count = HISTORY_CAPACITY - (history.start - history.current) + 2;
        }
    } else {
        history.start = HISTORY_NEXT(history.start);
    }

    history.current = HISTORY_NEXT(history.current);
    memcpy(history.entries[history.current], link, HTML_LINK_SIZE);
    history.entries[history.current][HTML_LINK_SIZE] = '\0';
}

char* history_next_link()
{
    if (history.count == 0)
        return NULL;

    if (history_at_last_link())
        return NULL;

    history.current = HISTORY_NEXT(history.current);
    return HISTORY_CURRENT_LINK;
}

char* history_prev_link()
{
    if (history.count == 0 || history.current == history.start)
        return NULL;

    history.current = HISTORY_PREV(history.current);
    return HISTORY_CURRENT_LINK;
}

// Ugly way to handle load errors in the drawer
void drawer_history_error()
{
    history.current = HISTORY_NEXT(history.current);
}
