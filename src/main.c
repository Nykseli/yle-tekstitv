
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <tidy.h>
#include <tidybuffio.h>
#include <unistd.h>

#include "drawer.h"
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

    drawer drawer;
    init_drawer(&drawer);
    draw_parser(&drawer, &parser);
    free_drawer(&drawer);

    free_html_parser(&parser);
    return 0;
}
