#ifndef WM_API_H
#define WM_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

#ifndef WM_API
#  if defined(_WIN32)
#    ifdef WM_API_EXPORT
#      define WM_API __declspec(dllexport)
#    else
#      define WM_API __declspec(dllimport)
#    endif
#  else
#    ifdef WM_API_EXPORT
#      define WM_API __attribute__((visibility("default")))
#    else
#      define WM_API
#    endif
#  endif
#endif

typedef struct wm_node wm_node;
typedef struct wm_toplevel wm_toplevel;
typedef struct wm_output wm_output;

typedef struct wm_buffer wm_buffer;

typedef uint64_t wm_id_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} wm_box_t;

typedef uint32_t wm_button_t;

typedef enum {
    WM_RESIZE_EDGE_INVALID = -1,
    WM_RESIZE_EDGE_NONE   = 0,
    WM_RESIZE_EDGE_TOP    = 1 << 0, // 1
    WM_RESIZE_EDGE_BOTTOM = 1 << 1, // 2
    WM_RESIZE_EDGE_LEFT   = 1 << 2, // 4
    WM_RESIZE_EDGE_RIGHT  = 1 << 3  // 8
} wm_resize_edges_bits_t;

typedef enum {
    WM_OK = 0,
    WM_FAILED_TO_CREATE_SCENE_BUFFER,
} wm_result_t;

typedef enum {
    WM_MODIFIER_SHIFT = 1 << 0,
	WM_MODIFIER_CAPS = 1 << 1,
	WM_MODIFIER_CTRL = 1 << 2,
	WM_MODIFIER_ALT = 1 << 3,
	WM_MODIFIER_MOD2 = 1 << 4,
	WM_MODIFIER_MOD3 = 1 << 5,
	WM_MODIFIER_LOGO = 1 << 6,
	WM_MODIFIER_MOD5 = 1 << 7,
} wm_modifier_mask_t;

typedef enum {
    WM_REQUEST_MOVE = 0,
    WM_REQUEST_RESIZE,
    WM_REQUEST_MAXIMIZE,
    WM_REQUEST_FULLSCREEN,
    WM_REQUEST_MINIMIZE,
    WM_REQUEST_ACTIVATE,
    WM_REQUEST_CLOSE
} wm_toplevel_request_type_t;

typedef struct {
    wm_toplevel_request_type_t type;
    wm_toplevel *toplevel;
    void *data;
} wm_toplevel_request_event_t;

typedef struct {
    uint32_t keysym;  /* xkb_keysym_t */

    uint32_t raw_code;  
    uint32_t modifiers; 

    bool pressed;
} wm_keyboard_event_t;

typedef struct {
    double global_x;   /* layout/global coords */
    double global_y;
    double local_x;    /* local coordinates, wayland */
    double local_y;
    wm_node *node;
} wm_node_at_coords_t;

typedef struct {
    double global_x; 
    double global_y;

    wm_button_t button; 
    bool pressed;
} wm_pointer_event_t;

typedef struct {
    uint32_t edges; // e.g. WM_RESIZE_EDGE_TOP | WM_RESIZE_EDGE_LEFT
} wm_resize_request_payload;

typedef struct {
    bool state;     // true = enable (Max/Full/Min), false = disable (Restore)
} wm_toggle_request_payload;

typedef void (*wm_on_toplevel_map)(wm_toplevel *t);
typedef void (*wm_on_toplevel_unmap)(wm_toplevel *t);
typedef void (*wm_on_toplevel_commit)(wm_toplevel *t);
typedef void (*wm_on_toplevel_geometry)(wm_toplevel *t, wm_box_t old_geo, wm_box_t new_geo);
typedef void (*wm_on_toplevel_request_close)(wm_toplevel *t);

//returns bool to notify if the key has been processed or needs to be passed to the seat
typedef bool (*wm_on_keyboard_key)(wm_keyboard_event_t *ev);

typedef bool (*wm_on_pointer_move)(wm_pointer_event_t *ev);
typedef bool (*wm_on_pointer_button)(wm_pointer_event_t *ev);

typedef void (*wm_on_toplevel_request_event)(wm_toplevel_request_event_t *ev);

typedef struct {
    wm_on_toplevel_map on_map;
    wm_on_toplevel_unmap on_unmap;
    wm_on_toplevel_commit on_commit;
    wm_on_toplevel_geometry on_geometry;
    wm_on_toplevel_request_close on_close;

    wm_on_keyboard_key on_key;

    wm_on_pointer_move on_pointer_move;
    wm_on_pointer_button on_pointer_button;

    wm_on_toplevel_request_event on_toplevel_request_event;
} wm_callbacks_t;

WM_API bool wm_register(wm_callbacks_t *cbs, void *user_data);
WM_API void wm_unregister();

WM_API void wm_plugin_log(const char *fmt, ...);

typedef void (*wm_toplevel_iter_cb)(wm_toplevel *t, void *user);
WM_API void wm_foreach_toplevel(wm_toplevel_iter_cb cb, void *user);

WM_API wm_toplevel **wm_get_toplevels(size_t *size); 
WM_API void wm_unref_toplevels(wm_toplevel **t, size_t amnt);

WM_API wm_toplevel *wm_get_focused_toplevel(void);
WM_API void wm_unref_toplevel(wm_toplevel *);

WM_API wm_node_at_coords_t *wm_try_get_node_at_coords(double x, double y);
WM_API void wm_unref_node_at_coords(wm_node_at_coords_t *coords);

WM_API wm_toplevel *wm_try_get_toplevel_from_node(wm_node*);
WM_API wm_buffer *wm_try_get_buffer_from_node(wm_node*);
WM_API uint32_t wm_try_get_resize_grip(wm_node *, wm_toplevel **);

WM_API wm_output *wm_get_focused_output();
WM_API wm_output *wm_get_output_of_toplevel(wm_toplevel *t);
WM_API void wm_unref_output(wm_output *);

WM_API wm_toplevel* wm_get_next_toplevel(struct wm_toplevel *cur);

WM_API void wm_focus_toplevel(wm_toplevel *t);
WM_API void wm_raise_toplevel(wm_toplevel *t);
WM_API void wm_lower_toplevel(wm_toplevel *t);

WM_API void wm_begin_move(wm_toplevel *t);
//Full box ( considering decoration )
WM_API void wm_begin_resize(wm_toplevel *t, uint32_t edge_bits);
WM_API void wm_cancel_window_op();
WM_API void wm_set_cursor(const char*);

WM_API void wm_set_toplevel_position(wm_toplevel *t, int x, int y);
WM_API void wm_set_toplevel_geometry(wm_toplevel *t, wm_box_t geo);
WM_API wm_box_t wm_restore_toplevel_geometry(wm_toplevel *t);

WM_API wm_box_t wm_get_toplevel_geometry(wm_toplevel *t);

WM_API wm_box_t wm_get_output_geometry(wm_output *output);
WM_API wm_box_t wm_get_output_usable_area(wm_output *output);
WM_API float wm_get_output_render_scale(wm_output *output);

WM_API void wm_hide_toplevel(wm_toplevel *t);
WM_API void wm_unhide_toplevel(wm_toplevel *t);
WM_API void wm_close_toplevel(wm_toplevel *t);

WM_API void wm_configure_toplevel_resize_grips(wm_toplevel *t, 
                                                 int off_x, int off_y, 
                                                 int width, int height, 
                                                 int grip_thickness);

WM_API void wm_configure_toplevel_resize_grips_with_color(wm_toplevel *t, 
                                                 int off_x, int off_y, 
                                                 int width, int height, 
                                                 int grip_thickness,
                                                 float color[4]);


WM_API const char *wm_get_toplevel_title(wm_toplevel *t);
WM_API uint64_t wm_get_toplevel_id(wm_toplevel *t);

WM_API void wm_set_toplevel_fullscreen(wm_toplevel *t, bool f);
WM_API void wm_set_toplevel_maximized(wm_toplevel *t, bool m);

WM_API bool wm_toplevel_is_fullscreen(wm_toplevel *t);
WM_API bool wm_toplevel_is_maximized(wm_toplevel *t);
WM_API bool wm_toplevel_is_hidden(wm_toplevel *t);
WM_API bool wm_toplevel_is_mapped(wm_toplevel *t);
WM_API bool wm_toplevel_is_csd(wm_toplevel *t);

WM_API bool wm_toplevel_wants_maximize(wm_toplevel *t);
WM_API bool wm_toplevel_wants_fullscreen(wm_toplevel *t);

WM_API wm_buffer *wm_create_buffer(int width, int height, bool cpu_buffer);
WM_API void wm_destroy_buffer(wm_buffer *b);

WM_API wm_toplevel *wm_get_toplevel_of_buffer(wm_buffer *b);

typedef struct {
    void *ptr;
    uint32_t format;
    size_t stride;
} wm_buffer_data;

/*
//in case we want to draw something to a cpu buffer, this possible
//example for format XRGB8888
wm_buffer_data data = wm_buffer_lock_data(buffer);

for (int y = 0; y < geometry.height; ++y) { 
    uint32_t *row = (uint32_t *)data.ptr + (size_t)y * (data.stride/4);
    for (int x = 0; x < geometry.width; ++x) {
        row[x] = 0xFF666666;
    }
}
wm_buffer_release_data(buffer);*/
WM_API wm_buffer_data wm_lock_buffer_data(wm_buffer *buffer);
WM_API void wm_release_buffer_data(wm_buffer *buffer);

typedef void (*wm_render_cb)(void *user_data);
WM_API bool wm_render_fn_to_buffer(wm_buffer *buffer, wm_render_cb cb, void *user_data);

WM_API wm_box_t wm_get_buffer_geometry(wm_buffer *buffer);
WM_API void wm_set_buffer_geometry(wm_buffer *, wm_box_t geo);

WM_API wm_buffer *wm_update_overlay(const char *name, wm_buffer *buffer, int x, int y);
WM_API wm_buffer *wm_remove_overlay(const char *name);

WM_API wm_buffer *wm_toplevel_attach_buffer(wm_toplevel *toplevel, wm_buffer *buffer, int x, int y);

WM_API void wm_toplevel_attach_state(wm_toplevel *toplevel, void *data);

WM_API void *wm_get_toplevel_state(wm_toplevel *toplevel);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* WM_API_H */

