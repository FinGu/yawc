#include "wm_api.h"

void on_toplevel_move_request(wm_toplevel_request_event_t *event);
void on_toplevel_resize_request(wm_toplevel_request_event_t *event);
void on_toplevel_maximize_request(wm_toplevel_request_event_t *event);
void on_toplevel_fullscreen_request(wm_toplevel_request_event_t *event);
void on_toplevel_minimize_request(wm_toplevel_request_event_t *event);
void on_toplevel_activate_request(wm_toplevel_request_event_t *event);
void on_toplevel_close_request(wm_toplevel_request_event_t *event);

