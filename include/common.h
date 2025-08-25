#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>

#define BUF_SIZE 1024

static inline void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
#endif
