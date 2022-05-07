#ifndef _UTIL_H_
#define _UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

void add_history_link(char* link);
char* history_next_link();
char* history_prev_link();
void drawer_history_error();

#ifdef __cplusplus
}
#endif

#endif
