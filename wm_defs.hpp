#include "toplevel.hpp"
#include "layer.hpp"
#include "wm_api.h"

typedef struct wm_toplevel {
    yawc_toplevel *toplevel;
} wm_toplevel;

typedef struct wm_output {
    yawc_output *output;
} wm_output;

typedef struct wm_node {
    wlr_scene_node *node;
} wm_node;

typedef struct wm_buffer{
    enum { GL_FOR_TOPLEVEL, GL_OVERLAY } kind;

    yawc_toplevel *toplevel;

    bool mapped;

    struct wlr_buffer *buffer;

    wm_box_t box;

} wm_buffer;

wm_keyboard_event_t wm_create_empty_keyboard_event();

wm_pointer_event_t wm_create_pointer_event(yawc_server *, uint32_t button, bool pressed);
wm_toplevel_request_event_t wm_create_toplevel_request_event(wm_toplevel *toplevel, wm_toplevel_request_type_t type, void *data);
wm_toplevel *wm_create_toplevel(yawc_toplevel *toplevel);

void wm_destroy_pointer_event(wm_pointer_event_t *ev);
void wm_destroy_keyboard_event(wm_keyboard_event_t *ev);
void wm_destroy_toplevel_request_event(wm_toplevel_request_event_t *ev);

wm_node *wm_create_node(struct wlr_scene_node *node);
