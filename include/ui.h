#ifndef UI_H
#define UI_H

void ui_init(const char *self_name, int is_host);

void ui_handle_line(const char *line);

void ui_handle_peer_line(const char *line);

#endif
