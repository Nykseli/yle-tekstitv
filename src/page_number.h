#ifndef _PAGE_NUMBER_H_
#define _PAGE_NUMBER_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drawer.h"
#include "html_parser.h"
#include "page_loader.h"

extern char https_page_url[41];
extern char relative_page_url[35];

int page_number(const char* page);
int subpage_number(const char* subpage);
void add_page(int page);
void add_subpage(int subpage);
char* get_page();

#endif
