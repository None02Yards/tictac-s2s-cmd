#include <stdio.h>
#include "game.h"

static char B[9];
static char cur, win;
static int  moves;

static char check_winner(void){
    static const int W[8][3]={{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
    for (int i=0;i<8;i++){
        char a=B[W[i][0]], b=B[W[i][1]], c=B[W[i][2]];
        if ((a=='X'||a=='O') && a==b && b==c) return a;
    }
    return '\0';
}

void game_reset(void){
    for (int i=0;i<9;i++) B[i] = '1'+i;
    cur='X'; win='\0'; moves=0;
}

char game_current(void){ return cur; }

int game_move(int pos){
    if (pos<1||pos>9 || win) return 0;
    char *cell=&B[pos-1];
    if (*cell=='X'||*cell=='O') return 0;
    *cell=cur; moves++;
    win = check_winner();
    if (!win && moves>=9) win='D';
    if (!win) cur = (cur=='X'?'O':'X');
    return 1;
}

char game_winner(void){ return win; }
int  game_over(void){ return win!='\0'; }

void game_board9(char out[10]){
    for (int i=0;i<9;i++) out[i]=B[i];
    out[9]='\0';
}

void game_board_ascii(char out[64]){
    snprintf(out,64," %c | %c | %c\n---+---+---\n %c | %c | %c\n---+---+---\n %c | %c | %c",
        B[0],B[1],B[2], B[3],B[4],B[5], B[6],B[7],B[8]);
}
