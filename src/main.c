// src/main.c
#include <string.h>
#include <stdlib.h>      // atoi
#include <unistd.h>
#include <sys/select.h>
#include "common.h"
#include "protocol.h"
#include "s2s.h"
#include "game.h"
#include "ui.h"

static void on_peer_line(const char *line) {
    ui_handle_peer_line(line);
}

static void usage(const char *p){
    fprintf(stderr,
      "Usage:\n"
      "  %s --host <port> --name <me>\n"
      "  %s --connect <ip> <port> --name <me>\n", p, p);
    exit(1);
}

int main(int argc, char **argv){
    const char *name = "Server";
    int is_host = 0, port = DEFAULT_PORT;
    const char *ip = NULL;

    // light CLI
    for (int i=1;i<argc;i++){
        if (!strcmp(argv[i],"--host") && i+1<argc){ is_host=1; port=atoi(argv[++i]); }
        else if (!strcmp(argv[i],"--connect") && i+2<argc){ ip=argv[++i]; port=atoi(argv[++i]); }
        else if (!strcmp(argv[i],"--name") && i+1<argc){ name=argv[++i]; }
        else usage(argv[0]);
    }
    if (!is_host && !ip) usage(argv[0]);

    game_reset();              
    ui_init(name, is_host);      
    s2s_set_on_line(on_peer_line);

    // bring up the link
    if (is_host) {
        if (s2s_listen_start(port)!=0) die("listen");
    } else {
        if (s2s_connect_start(ip, port)!=0) die("connect");
    }

    s2s_sendf("%s %s", MSG_HELLO, name);
    

    char buf[BUF_SIZE];
    while (1){
        fd_set rfds; FD_ZERO(&rfds);
        FD_SET(0, &rfds); // stdin
        if (select(1, &rfds, NULL, NULL, NULL) < 0) die("select");
        if (FD_ISSET(0, &rfds)) {
            if (!fgets(buf, sizeof(buf), stdin)) break;
            buf[strcspn(buf, "\r\n")] = 0;
            if (!buf[0]) continue;
            ui_handle_line(buf);
        }
    }

    s2s_stop();
    return 0;
}
