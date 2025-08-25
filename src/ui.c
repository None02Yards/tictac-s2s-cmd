// src/ui.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#include "ui.h"
#include "s2s.h"
#include "protocol.h"
#include "game.h"
#include "bot.h"
#include "storage.h"



static char SELF[64];
static int IS_HOST = 0;

static int json_has(const char *line, const char *key, const char *val){
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\":\"%s\"", key, val);
    return strstr(line, needle) != NULL;
}
static int json_get_str(const char *line, const char *key, char *out, size_t n){
    char k[64]; snprintf(k, sizeof(k), "\"%s\":\"", key);
    const char *p = strstr(line, k);
    if (!p) return 0;
    p += strlen(k);
    const char *q = strchr(p, '"');
    if (!q) return 0;
    size_t len = (size_t)(q - p);
    if (len >= n) len = n-1;
    memcpy(out, p, len); out[len] = '\0';
    return 1;
}
static int json_get_int(const char *line, const char *key, int *out){
    char k[64]; snprintf(k, sizeof(k), "\"%s\":", key);
    const char *p = strstr(line, k);
    if (!p) return 0;
    p += strlen(k);
    *out = atoi(p);
    return 1;
}



// Round state
static int game_active = 0;
static int game_paused = 0;

static int scoreX = 0, scoreO = 0, scoreD = 0;

static int GREETED = 0;

static int ping_outstanding = 0;
static unsigned int ping_id = 0;
static struct timespec ping_ts;

static inline char my_role(void)   { return IS_HOST ? 'X' : 'O'; }
static inline char peer_role(void) { return IS_HOST ? 'O' : 'X'; }

static void now_mono(struct timespec *ts){
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, ts);
#elif defined(TIME_UTC)
    timespec_get(ts, TIME_UTC);
#else
    struct timeval tv; gettimeofday(&tv,NULL);
    ts->tv_sec = tv.tv_sec; ts->tv_nsec = (long)tv.tv_usec*1000L;
#endif
}

static long rtt_ms(const struct timespec *t0, const struct timespec *t1){
    long sec  = (long)(t1->tv_sec  - t0->tv_sec);
    long nsec = (long)(t1->tv_nsec - t0->tv_nsec);
    return sec*1000 + nsec/1000000;
}

static void print_turn_hint(char who){
    if (!game_active) {
        if (game_paused) {
            printf("⏸️  Round paused (peer offline). Type: /rematch or /exit");
            if (bot_is_enabled()) printf(" or keep playing vs bot.");
            printf("\n");
        } else {
            printf("➡️  No active round. Type: /play\n");
        }
        return;
    }
    if (who == my_role()) {
        printf("👉 Your turn (%c). Type: /move <1-9>\n", who);
    } else if (who=='X' || who=='O') {
        if (s2s_is_connected())
            printf("⏳ Waiting for %c...\n", who);
        else if (bot_is_enabled())
            printf("🤖 Bot (%c) is thinking...\n", who);
        else
            printf("⏳ Waiting for %c (peer offline)…\n", who);
    }
}

static void print_scoreboard(void){
    printf("\n====== SCOREBOARD ======\n");
    printf("X Wins : %d\n", scoreX);
    printf("O Wins : %d\n", scoreO);
    printf("Draws  : %d\n", scoreD);
    printf("========================\n\n");
}

static void bot_take_turn_if_needed(void){
    if (!game_active) return;
    if (s2s_is_connected()) return;
    if (!bot_is_enabled()) return;
    if (game_over()) return;
    if (game_current() != peer_role()) return; 

    int pos = bot_pick_move();
    if (pos <= 0) return;

    if (!game_move(pos)) return;

    char ascii[64]; game_board_ascii(ascii);
    printf("[bot]\n%s\n", ascii);

    if (game_over()) {
        char w = game_winner();
        if      (w=='X') scoreX++;
        else if (w=='O') scoreO++;
        else             scoreD++;
        const char *msg = (w=='D') ? "Draw" : (w=='X' ? "X wins" : "O wins");
        printf("[result] %s — type /play for a new round.\n", msg);
        print_scoreboard();
        game_active = 0; game_paused = 0;
    } else {
        print_turn_hint(game_current());
    }
}

void ui_init(const char *self_name, int is_host){
    snprintf(SELF, sizeof(SELF), "%s", self_name);
    IS_HOST = is_host;
    GREETED = 0;
    ping_outstanding = 0;
    game_active = 0; game_paused = 0;
    bot_set_enabled(0);            // off by default
    bot_set_role(peer_role());     // bot  mimics the peer
    storage_load_scores(&scoreX, &scoreO, &scoreD);

    printf("=== S2S TicTacToe (%s) ===\n", IS_HOST ? "HOST (X)" : "CLIENT (O)");
    printf("Commands:\n");
    printf("  /play                - start a new round (keep scores)\n");
    printf("  /rematch             - alias: /play\n");
    printf("  /exit                - abandon paused round\n");
    printf("  /move <1-9>          - play a move (only when active & your turn)\n");
    printf("  /board               - show local board\n");
    printf("  /reset               - reset current round (keep scores)\n");
    printf("  /score               - show scoreboard\n");
    printf("  /bot on|off          - enable bot takeover when peer offline\n");
    printf("  /hello               - send greeting\n");
    printf("  /ping                - check peer is online (RTT)\n");
    printf("--------------------------\n");
    printf("➡️  No active round. Type: /play\n");
}

void ui_handle_line(const char *line){
    if (!line || !*line) return;

    if (!strncmp(line,"/play",5) || !strncmp(line,"/rematch",8)) {
        game_reset();
        game_active = 1; game_paused = 0;
        char nine[10]; game_board9(nine);
        s2s_sendf("%s %s", MSG_BOARD, nine);
        s2s_sendf("%s %c", MSG_TURN, game_current());
        char ascii[64]; game_board_ascii(ascii);
        printf("[start]\n%s\n", ascii);
        print_turn_hint(game_current());
        bot_take_turn_if_needed();
        return;
    }
    if (!strncmp(line,"/exit",5)) {
        if (game_paused) {
            game_active = 0; game_paused = 0;
            printf("Round abandoned. Type /play to start again.\n");
        } else {
            printf("No paused round.\n");
        }
        return;
    }

    // Bot 
    if (!strncmp(line,"/bot ",5)) {
        if (strstr(line+5,"on")) {
            bot_set_enabled(1);
            bot_set_role(peer_role());
            printf("🤖 Bot enabled for peer role %c.\n", peer_role());
            bot_take_turn_if_needed();
        } else if (strstr(line+5,"off")) {
            bot_set_enabled(0);
            printf("🤖 Bot disabled.\n");
        } else {
            printf("Usage: /bot on|off\n");
        }
        return;
    }

    if (!strncmp(line,"/hello",6)) {
        s2s_sendf("%s %s", MSG_HELLO, SELF);
        printf("[you] hello sent.\n");
        GREETED = 1;
        return;
    }

    if (!strncmp(line,"/ping",5)) {
        if (ping_outstanding) { printf("[ping] already waiting for PONG #%u…\n", ping_id); return; }
        ping_id++;
        now_mono(&ping_ts); ping_outstanding = 1;
        s2s_sendf("%s %u", MSG_PING, ping_id);
        printf("[ping] sent #%u\n", ping_id);
        return;
    }

    if (!strncmp(line,"/score",6)) { print_scoreboard(); print_turn_hint(game_current()); return; }

    if (!strncmp(line,"/board",6)) {
        if (!game_active) { printf("No active round. Type /play\n"); return; }
        char ascii[64]; game_board_ascii(ascii);
        printf("%s\n", ascii);
        print_turn_hint(game_current());
        return;
    }

    if (!strncmp(line,"/reset",6)) {
        game_reset();
        game_active = 1; game_paused = 0;
        char nine[10]; game_board9(nine);
        s2s_sendf("%s %s", MSG_BOARD, nine);
        s2s_sendf("%s %c", MSG_TURN, game_current());
        printf("Round reset.\n");
        char ascii[64]; game_board_ascii(ascii);
        printf("%s\n", ascii);
        print_turn_hint(game_current());
        bot_take_turn_if_needed();
        return;
    }

    // Moves
    if (!strncmp(line,"/move",5)) {
        if (!game_active) { printf("No active round. Type /play\n"); return; }
        if (game_paused)  { printf("Round paused; /rematch or /exit. "); if (bot_is_enabled()) printf("Or let bot play.\n"); else printf("\n"); return; }


        if (game_current() != my_role()) {
            printf("Not your turn. It is %c's turn.\n", game_current());
            return;
        }

        int pos = atoi(line+6);
        if (pos < 1 || pos > 9) { printf("Position must be 1..9.\n"); return; }
        if (!game_move(pos))    { printf("Invalid move (occupied or bad position).\n"); return; }

        char nine[10]; game_board9(nine);
        s2s_sendf("%s %d", MSG_MOVE, pos);
        s2s_sendf("%s %s", MSG_BOARD, nine);

        if (game_over()) {
            char w = game_winner();
            s2s_sendf("%s %c", MSG_RESULT, w);
            if      (w=='X') scoreX++;
            else if (w=='O') scoreO++;
            else             scoreD++;

            char ascii[64]; game_board_ascii(ascii);
            printf("[local]\n%s\n", ascii);
            const char *msg = (w=='D') ? "Draw" : (w=='X' ? "X wins" : "O wins");
            printf("[result] %s — type /play for a new round.\n", msg);
            print_scoreboard();
            game_active = 0; game_paused = 0;
        } else {
            s2s_sendf("%s %c", MSG_TURN, game_current());
            char ascii[64]; game_board_ascii(ascii);
            printf("[local]\n%s\n", ascii);
            print_turn_hint(game_current());
            bot_take_turn_if_needed();
        }
        return;
    }

    // Plain chat
    s2s_sendf("%s %s", MSG_CHAT, line);
}



void ui_handle_peer_line(const char *line){
    if (!line) return;

    /* - JSON mode - */
    if (line[0] == '{') {
        if (json_has(line, "t", "HELLO")) {
            char name[64]; if (json_get_str(line, "name", name, sizeof(name))) {
                printf("[peer] %s says hello. You are %c, peer is %c.\n", name, my_role(), peer_role());
                if (!GREETED) {
#ifdef PROTO_JSON
                    s2s_sendf("{\"t\":\"HELLO\",\"name\":\"%s\"}", SELF);
#else
                    s2s_sendf("%s %s", MSG_HELLO, SELF);
#endif
                    GREETED = 1;
                }
            }
            return;
        }
        if (json_has(line, "t", "PING")) {
            int id=0; json_get_int(line, "id", &id);
#ifdef PROTO_JSON
            s2s_sendf("{\"t\":\"PONG\",\"id\":%d}", id);
#else
            s2s_sendf("%s %d", MSG_PONG, id);
#endif
            return;
        }
        if (json_has(line, "t", "PONG")) {
            int id=0; json_get_int(line, "id", &id);
            if (ping_outstanding && id == (int)ping_id) {
                struct timespec now; now_mono(&now);
                printf("[pong] peer online — RTT ~ %ld ms (id #%d)\n", rtt_ms(&ping_ts,&now), id);
                ping_outstanding = 0;
            } else {
                printf("[pong] received (id #%d)\n", id);
            }
            return;
        }
        if (json_has(line, "t", "CHAT")) {
            char text[512]; if (json_get_str(line, "text", text, sizeof(text))) {
                printf("[peer] %s\n", text);
            }
            return;
        }
        if (json_has(line, "t", "MOVE")) {
            int pos=0; json_get_int(line, "pos", &pos);
            if (!game_move(pos)) { printf("[warn] peer MOVE %d invalid locally.\n", pos); return; }
            char ascii[64]; game_board_ascii(ascii);
            printf("[peer]\n%s\n", ascii);
            return; 
        }
        if (json_has(line, "t", "BOARD")) {
            game_active = 1; game_paused = 0;
            char ascii[64]; game_board_ascii(ascii);
            printf("[start]\n%s\n", ascii);
            return;
        }
        if (json_has(line, "t", "TURN")) {
            char tbuf[4]; if (json_get_str(line, "turn", tbuf, sizeof(tbuf))) {
                print_turn_hint(tbuf[0]);
                bot_take_turn_if_needed();
            }
            return;
        }
        if (json_has(line, "t", "RESULT")) {
            char wbuf[4]; if (json_get_str(line, "winner", wbuf, sizeof(wbuf))) {
                char w = wbuf[0];
                if      (w=='X') scoreX++;
                else if (w=='O') scoreO++;
                else             scoreD++;
                const char *msg = (w=='D') ? "Draw" : (w=='X' ? "X wins" : "O wins");
                printf("[result] %s — type /play for a new round.\n", msg);
                print_scoreboard();
                storage_save_scores(scoreX, scoreO, scoreD);
                game_active = 0; game_paused = 0;
            }
            return;
        }
        if (json_has(line, "t", "STATUS")) {
            char state[32]; if (json_get_str(line, "state", state, sizeof(state))) {
                if (!strcmp(state,"DISCONNECTED")) {
                    printf("[net] Peer lost; reconnecting…\n");
                    if (game_active) {
                        if (bot_is_enabled()) {
                            printf("[bot] Taking over for %c.\n", peer_role());
                            bot_set_role(peer_role());
                            bot_take_turn_if_needed();
                        } else {
                            game_paused = 1; game_active = 0;
                            print_turn_hint(game_current());
                            printf("Do you want to /rematch or /exit the game?\n");
                        }
                    }
                } else if (!strcmp(state,"RECONNECTED")) {
                    printf("[net] Peer reconnected.\n");
                    if (!game_active) print_turn_hint(game_current());
                }
            }
            return;
        }

        printf("[peer-json] %s\n", line);
        return;
    }


    // STATUS DISCONNECTED / RECONNECTED
    if (!strncmp(line, MSG_STATUS, strlen(MSG_STATUS))) {
        const char *p = line + 7;
        if (strstr(p,"DISCONNECTED")) {
            printf("[net] Peer lost; reconnecting…\n");
            if (game_active) {
                if (bot_is_enabled()) {
                    printf("[bot] Taking over for %c.\n", peer_role());
                    bot_set_role(peer_role());
                    bot_take_turn_if_needed();
                } else {
                    game_paused = 1; game_active = 0;
                    print_turn_hint(game_current());
                    printf("Do you want to /rematch or /exit the game?\n");
                }
            }
        } else if (strstr(p,"RECONNECTED")) {
            printf("[net] Peer reconnected.\n");
            if (!game_active) print_turn_hint(game_current());
        }
        return;
    }

    if (!strncmp(line, MSG_HELLO, strlen(MSG_HELLO))) {
        const char *peer = line + 6;
        printf("[peer] %s says hello. You are %c, peer is %c.\n", peer, my_role(), peer_role());
        if (!GREETED) { s2s_sendf("%s %s", MSG_HELLO, SELF); GREETED = 1; }
        return;
    }

    if (!strncmp(line, MSG_PING, strlen(MSG_PING))) {
        unsigned int id = 0; sscanf(line + 5, "%u", &id);
        s2s_sendf("%s %u", MSG_PONG, id);
        return;
    }
    if (!strncmp(line, MSG_PONG, strlen(MSG_PONG))) {
        unsigned int id = 0; sscanf(line + 5, "%u", &id);
        if (ping_outstanding && id == ping_id) {
            struct timespec now; now_mono(&now);
            printf("[pong] peer online — RTT ~ %ld ms (id #%u)\n", rtt_ms(&ping_ts,&now), id);
            ping_outstanding = 0;
        } else {
            printf("[pong] received (id #%u)\n", id);
        }
        return;
    }

    if (!strncmp(line, MSG_CHAT, strlen(MSG_CHAT))) { printf("[peer] %s\n", line + 5); return; }

    if (!strncmp(line, MSG_MOVE, strlen(MSG_MOVE))) {
        int pos = atoi(line + 5);
        if (!game_move(pos)) { printf("[warn] peer MOVE %d invalid locally.\n", pos); return; }
        char ascii[64]; game_board_ascii(ascii);
        printf("[peer]\n%s\n", ascii);
        return; 
    }

    if (!strncmp(line, MSG_BOARD, strlen(MSG_BOARD))) {
        game_active = 1; game_paused = 0;
        char ascii[64]; game_board_ascii(ascii);
        printf("[start]\n%s\n", ascii);
        return;
    }

    if (!strncmp(line, MSG_TURN, strlen(MSG_TURN))) {
        char t = *(line + 5);
        print_turn_hint(t);
        bot_take_turn_if_needed();
        return;
    }

    if (!strncmp(line, MSG_RESULT, strlen(MSG_RESULT))) {
        char w = *(line + 7);
        if      (w=='X') scoreX++;
        else if (w=='O') scoreO++;
        else             scoreD++;
        const char *msg = (w=='D') ? "Draw" : (w=='X' ? "X wins" : "O wins");
        printf("[result] %s — type /play for a new round.\n", msg);
        print_scoreboard();
        storage_save_scores(scoreX, scoreO, scoreD);
        game_active = 0; game_paused = 0;
        return;
    }

    // Fallback
    printf("[peer-raw] %s\n", line);
}
