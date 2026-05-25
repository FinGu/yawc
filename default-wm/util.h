#include "wm_api.h"

#include <time.h>

void center_window(wm_toplevel *toplevel);

void maximize_window(wm_toplevel *toplevel);

void restore_maximized_window(wm_toplevel *toplevel, double cursor_x, double cursor_y);

double get_time_diff(struct timespec end, struct timespec start);
