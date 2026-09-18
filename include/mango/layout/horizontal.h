#ifndef __HORIZONTAL_H__
#define __HORIZONTAL_H__

#include "mango/common/types.h"
#include <stdint.h>

void tile(Monitor *m);

void right_tile(Monitor *m);

void center_tile(Monitor *m);

void deck(Monitor *m);

void monocle(Monitor *m);

void grid(Monitor *m);

// Computes a row-major grid's column/row counts for n items: cols =
// ceil(sqrt(n)); rows shrinks by one if the last row would otherwise sit
// empty. overcols = n % cols, the item count in the final, short row (0 if
// n divides evenly) -- callers use it to identify/center that last row.
// Shared by grid() and the tag-grouped overview's region partition.
void compute_grid_dims(int32_t n, int32_t *cols, int32_t *rows,
					   int32_t *overcols);

void fair(Monitor *m);

#endif
