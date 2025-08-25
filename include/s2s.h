#ifndef S2S_H
#define S2S_H

#include <stdarg.h>
#include <stddef.h>

// Callback signature: receives complete lines from peer (without '\n')
typedef void (*s2s_on_line_cb)(const char *line);

// Start as host (listener). Returns 0 on success.
int  s2s_listen_start(int port);

// Start as connector (client). Returns 0 on success.
int  s2s_connect_start(const char *ip, int port);

// Register line callback (must be set before start)
void s2s_set_on_line(s2s_on_line_cb cb);

// Send a formatted line to peer (auto-appends '\n'). Returns 0 on success.
int  s2s_sendf(const char *fmt, ...);

// Is the link up?
int  s2s_is_connected(void);

// Stop and cleanup
void s2s_stop(void);

#endif
