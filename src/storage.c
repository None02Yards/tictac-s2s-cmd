#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "storage.h"

static void scores_path(char *out, size_t n){
    const char *home = getenv("HOME");
    if (home && *home) {
        snprintf(out, n, "%s/.s2s-ttt.scores", home);
    } else {
        snprintf(out, n, ".s2s-ttt.scores");
    }
}

int storage_load_scores(int *x, int *o, int *d){
    if (!x||!o||!d) return -1;
    *x = *o = *d = 0;

    char path[512]; scores_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    int tx=0,to=0,td=0;
    int ok = fscanf(f, "X %d\nO %d\nD %d\n", &tx, &to, &td);
    fclose(f);
    if (ok == 3) { *x = tx; *o = to; *d = td; return 0; }
    return -1;
}

int storage_save_scores(int x, int o, int d){
    char path[512]; scores_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    int ok = fprintf(f, "X %d\nO %d\nD %d\n", x, o, d);
    fclose(f);
    return (ok > 0) ? 0 : -1;
}
