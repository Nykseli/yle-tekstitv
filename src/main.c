
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <tidy.h>
#include <tidybuffio.h>
#include <unistd.h>

#include "html_parser.h"

static char* open_html(char* file)
{
    int fd = open(file, O_RDONLY);
    if (fd < 0)
        printf("Cannot open %s\n", file);

    struct stat fs;
    fstat(fd, &fs);

    char* buff = malloc(fs.st_size);
    read(fd, buff, fs.st_size);

    return buff;
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("Usage: %s <html file>\n", argv[0]);
        return 1;
    }

    html_parser parser;
    init_html_parser(&parser);
    const char* input = open_html(argv[1]);
    parse_html(&parser, input);
    printf("Title: %s\n", html_text_string(parser.title));

    printf("Top navigations:\n");
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        html_link link = parser.top_navigation[i];
        printf("Link link:%s\n", html_text_string(link.url));
        printf("Link inner text:%s\n", html_text_string(link.inner_text));
    }

    printf("Bottom navigations:\n");
    for (size_t i = 0; i < BOTTOM_NAVIGATION_SIZE; i++) {
        html_link link = parser.bottom_navigation[i];
        printf("Link link:%s\n", html_text_string(link.url));
        printf("Link inner text:%s\n", html_text_string(link.inner_text));
    }

    for (size_t i = 0; i < parser.middle_size; i++) {
        html_item item = parser.middle[i];
        if (item.type == HTML_LINK) {
            printf("Found link:\n");
            html_link link = html_item_as_link(item);
            printf("Buffer size: %ld\n", link.url.size);
            printf("Link link:%s\n", html_text_string(link.url));
            printf("Link inner text:%s\n", html_text_string(link.inner_text));
        } else if (item.type == HTML_TEXT) {
            printf("Found text:\n");
            html_text link = html_item_as_text(item);
            printf("Text text:%s\n", html_text_string(link));
        }
    }
    free_html_parser(&parser);
    return 0;
}
