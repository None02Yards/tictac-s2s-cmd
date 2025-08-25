// src/s2s.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "s2s.h"
#include "protocol.h"

static s2s_on_line_cb g_cb = NULL;

static int  g_listen_fd   = -1;
static int  g_sock_fd     = -1;
static int  g_connected   = 0;
static int  g_running     = 0;
static int  g_is_client   = 0;
static char g_last_ip[64] = {0};
static int  g_last_port   = 0;

static pthread_t g_rx_thread;
static pthread_mutex_t g_send_mtx = PTHREAD_MUTEX_INITIALIZER;

void s2s_set_on_line(s2s_on_line_cb cb) { g_cb = cb; }

static int send_all(int fd, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, buf + off, len - off, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

static int connect_once(const char *ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) { close(fd); return -1; }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void *rx_thread(void *arg) {
    (void)arg;
    char inbuf[4096];
    char linebuf[8192];
    size_t llen = 0;

    while (g_running) {
        ssize_t n = recv(g_sock_fd, inbuf, sizeof(inbuf), 0);
        if (n <= 0) {
            // peer lost
            if (g_connected) {
                g_connected = 0;
                if (g_cb) g_cb("STATUS DISCONNECTED");
            }

            // Reconnect logic
            if (g_is_client) {
                int backoff = 1;
                while (g_running && !g_connected) {
                    fprintf(stderr, "[s2s] peer lost; retrying in %ds...\n", backoff);
                    sleep(backoff);
                    if (backoff < 16) backoff *= 2;

                    int fd = connect_once(g_last_ip, g_last_port);
                    if (fd >= 0) {
                        g_sock_fd = fd;
                        g_connected = 1;
                        if (g_cb) g_cb("STATUS RECONNECTED");
                        llen = 0; // reset partial line
                        break;
                    }
                }
                if (!g_running) break;
                continue; // back to recv on new socket
            } else {
                fprintf(stderr, "[s2s] peer lost; waiting for new connection...\n");
                close(g_sock_fd);
                g_sock_fd = -1;

                int fd = accept(g_listen_fd, NULL, NULL);
                if (fd < 0) {
                    if (!g_running) break;
                    sleep(1);
                    continue;
                }
                g_sock_fd = fd;
                g_connected = 1;
                if (g_cb) g_cb("STATUS RECONNECTED");
                llen = 0;
                continue;
            }
        }

        for (ssize_t i = 0; i < n; i++) {
            char ch = inbuf[i];
            if (ch == '\n') {
                linebuf[llen] = '\0';
                if (g_cb) g_cb(linebuf);
                llen = 0;
            } else {
                if (llen + 1 < sizeof(linebuf)) {
                    linebuf[llen++] = ch;
                }
            }
        }
    }

    g_connected = 0;
    return NULL;
}

int s2s_listen_start(int port) {
    g_is_client = 0;
    g_last_ip[0] = '\0';
    g_last_port  = port;

    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (bind(g_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(g_listen_fd); g_listen_fd = -1; return -1;
    }
    if (listen(g_listen_fd, 1) < 0) {
        perror("listen"); close(g_listen_fd); g_listen_fd = -1; return -1;
    }

    printf("[s2s] listening on %d, waiting for peer...\n", port);
    g_sock_fd = accept(g_listen_fd, NULL, NULL);
    if (g_sock_fd < 0) {
        perror("accept"); close(g_listen_fd); g_listen_fd = -1; return -1;
    }
    printf("[s2s] peer connected.\n");

    g_running   = 1;
    g_connected = 1;
    if (pthread_create(&g_rx_thread, NULL, rx_thread, NULL) != 0) {
        perror("pthread_create"); close(g_sock_fd); g_sock_fd = -1; return -1;
    }
    return 0;
}

int s2s_connect_start(const char *ip, int port) {
    g_is_client = 1;
    strncpy(g_last_ip, ip, sizeof(g_last_ip)-1);
    g_last_ip[sizeof(g_last_ip)-1] = '\0';
    g_last_port = port;

    int fd = connect_once(ip, port);
    if (fd < 0) { perror("connect"); return -1; }

    printf("[s2s] connected to %s:%d\n", ip, port);
    g_sock_fd = fd;

    g_running   = 1;
    g_connected = 1;
    if (pthread_create(&g_rx_thread, NULL, rx_thread, NULL) != 0) {
        perror("pthread_create"); close(g_sock_fd); g_sock_fd = -1; return -1;
    }
    return 0;
}

int s2s_sendf(const char *fmt, ...) {
    if (!g_connected || g_sock_fd < 0) return -1;

    char line[2048];
    va_list ap; va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    size_t n = strlen(line);
    if (n == 0 || line[n-1] != '\n') {
        if (n + 1 < sizeof(line)) { line[n] = '\n'; line[n+1] = '\0'; n++; }
    }

    pthread_mutex_lock(&g_send_mtx);
    int rc = send_all(g_sock_fd, line, n);
    pthread_mutex_unlock(&g_send_mtx);
    return rc;
}

int s2s_is_connected(void) { return g_connected; }

void s2s_stop(void) {
    g_running = 0;
    if (g_sock_fd >= 0)   { shutdown(g_sock_fd, SHUT_RDWR); close(g_sock_fd); g_sock_fd = -1; }
    if (g_listen_fd >= 0) { close(g_listen_fd); g_listen_fd = -1; }
    if (g_connected) { pthread_join(g_rx_thread, NULL); }
    g_connected = 0;
}
