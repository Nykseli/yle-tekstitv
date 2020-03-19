
#include <assert.h>
#include <stddef.h>

#include "html_parser.h"
#include "page_number.h"

char https_page_url[] = "https://yle.fi/tekstitv/txt/xxx_00xx.htm";

int page_number(const char* page)
{
    assert(page);

    size_t len = strlen(page);
    if (strlen(page) > 3)
        return -1;

    for (size_t i = 0; i < len; i++) {
        if (page[i] < '0' || page[i] > '9')
            return -1;
    }

    int result = atoi(page);
    return result >= 100 && result < 1000 ? result : -1;
}

int subpage_number(const char* subpage)
{
    assert(subpage);

    size_t len = strlen(subpage);
    if (len > 2)
        return -1;

    for (size_t i = 0; i < len; i++) {
        if (subpage[i] < '0' || subpage[i] > '9')
            return -1;
    }

    int result = atoi(subpage);
    return result >= 1 && result < 100 ? result : -1;
}

void add_page(int page)
{
    assert(page >= 100 && page <= 999);

    // Pxxx part
    char c1 = (page / 100) + '0';
    char c2 = (page % 100 / 10) + '0';
    char c3 = (page % 10) + '0';
    https_page_url[28] = c1;
    https_page_url[29] = c2;
    https_page_url[30] = c3;
}

void add_subpage(int subpage)
{
    assert(subpage >= 1 && subpage <= 99);

    // _xx part
    char c1 = subpage > 9 ? (subpage % 100 / 10) + '0' : '0';
    char c2 = (subpage % 10) + '0';
    https_page_url[34] = c1;
    https_page_url[35] = c2;
}

void replace_link_part(char* link)
{
    strncpy(https_page_url + (41 - HTML_LINK_SIZE - 1), link, HTML_LINK_SIZE);
}
