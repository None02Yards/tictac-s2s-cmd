#ifndef STORAGE_H
#define STORAGE_H

// Returns 0 on success, -1 on error
int storage_load_scores(int *x, int *o, int *d);
int storage_save_scores(int x, int o, int d);

#endif
