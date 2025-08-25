#ifndef BOT_H
#define BOT_H

void bot_set_enabled(int on);
int  bot_is_enabled(void);

void bot_set_role(char role);

int  bot_pick_move(void);

#endif
