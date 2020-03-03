

#include "drawer.h"
#include "html_parser.h"
#include "page_loader.h"

int main(int argc, char** argv)
{
    html_parser parser;
    init_html_parser(&parser);
    load_page(&parser, "https://yle.fi/tekstitv/txt/P200_01.html");
    parse_html(&parser);

    drawer drawer;
    init_drawer(&drawer);
    draw_parser(&drawer, &parser);
    free_drawer(&drawer);

    free_html_parser(&parser);
    return 0;
}
