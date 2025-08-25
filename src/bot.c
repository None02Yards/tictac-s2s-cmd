// src/bot.c
#include <string.h>
#include "bot.h"
#include "game.h"

static int  g_enabled = 0;
static char g_role    = 'O';

void bot_set_enabled(int on) { g_enabled = on ? 1 : 0; }
int  bot_is_enabled(void)    { return g_enabled; }
void bot_set_role(char role) { g_role = (role=='X'?'X':'O'); }

// Simple heuristic: center > corner > edge (no deep search)
static int is_free(char c){ return !(c=='X' || c=='O'); }

int bot_pick_move(void){
    if (!g_enabled) return 0;
    if (game_current() != g_role) return 0;
    if (game_over()) return 0;

    char b[10]; game_board9(b);

    // 1) center
    if (is_free(b[4])) return 5;

    // 2) corners
    int corners[4] = {1,3,7,9};
    for (int i=0;i<4;i++) {
        int p = corners[i];
        if (is_free(b[p-1])) return p;
    }
    // 3) edges
    int edges[4] = {2,4,6,8};
    for (int i=0;i<4;i++) {
        int p = edges[i];
        if (is_free(b[p-1])) return p;
    }
    return 0;
}
