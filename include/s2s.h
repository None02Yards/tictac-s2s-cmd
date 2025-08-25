#ifndef S2S_H
#define S2S_H

#include <stdarg.h>
#include <stddef.h>

typedef void (*s2s_on_line_cb)(const char *line);

int  s2s_listen_start(int port);

int  s2s_connect_start(const char *ip, int port);

void s2s_set_on_line(s2s_on_line_cb cb);

int  s2s_sendf(const char *fmt, ...);

int  s2s_is_connected(void);

void s2s_stop(void);

#endif
