#include "wm_api.h"

#include <time.h>

void center_window(wm_toplevel *toplevel);

wm_box_t maximize_window(wm_toplevel *toplevel);

wm_box_t unmaximize_window(wm_toplevel *toplevel);

void fullscreen_window(wm_toplevel *toplevel, wm_output *output);

void unfullscreen_window(wm_toplevel *toplevel);

double get_time_diff(struct timespec end, struct timespec start);

void hide_and_pick_next(wm_toplevel *toplevel);
