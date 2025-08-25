#ifndef GAME_H
#define GAME_H

void game_reset(void);
char game_current(void);        // 'X' or 'O'
int  game_move(int pos);        // 1=ok,0=invalid
char game_winner(void);         // 'X','O','D' or '\0'
int  game_over(void);           // 1 if winner/draw
void game_board9(char out[10]); // nine chars + '\0'
void game_board_ascii(char out[64]); // pretty 3x3 ascii board

#endif
