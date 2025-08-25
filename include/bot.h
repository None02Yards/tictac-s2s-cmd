#ifndef BOT_H
#define BOT_H

// Enable/disable bot takeover when peer is offline
void bot_set_enabled(int on);
int  bot_is_enabled(void);

// Which role (mark) the bot should play: 'X' or 'O'
void bot_set_role(char role);

// Return best position (1..9) to play now (0 if none/illegal)
int  bot_pick_move(void);

#endif
