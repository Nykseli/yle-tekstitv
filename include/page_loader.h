/**
 * Do not directly include
 */

#ifndef _TEKSTITV
#error "html_parser.h should not be directly incuded. Include tekstitv.h instead"
#endif

#ifndef _PAGE_LOADER_H_
#define _PAGE_LOADER_H_

#include "html_parser.h"

void load_page(html_parser* parser, char* page);

#endif
