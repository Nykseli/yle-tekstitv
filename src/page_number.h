#ifndef _PAGE_NUMBER_H_
#define _PAGE_NUMBER_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char https_page_url[41];

int page_number(const char* page);
int subpage_number(const char* subpage);
void add_page(int page);
void add_subpage(int subpage);
void replace_link_part(char* link);
char* get_page();

#endif
