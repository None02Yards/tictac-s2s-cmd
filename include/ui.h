#ifndef UI_H
#define UI_H

// Initialize UI (e.g., banner)
void ui_init(const char *self_name, int is_host);

// Handle one stdin line (no trailing newline). Calls s2s_sendf() for outbound.
void ui_handle_line(const char *line);

// Handle one inbound s2s line (parsed & prints board/status)
void ui_handle_peer_line(const char *line);

#endif
