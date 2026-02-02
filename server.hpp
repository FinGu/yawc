#include <string>
#include <map>

#include <wayland-client.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <xcb/xcb.h>

#include "config.hpp"
#include "wm_api.h"

#define static

#define class class_
#define namespace namespace_

extern "C" {
#include <wlr/backend.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <wlr/backend/libinput.h>
#include <wlr/backend/drm.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/render/dmabuf.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/util/region.h>
#include <wlr/util/log.h>
#include "extra/hyprland-global-shortcuts-v1.h"
}

#undef class
#undef namespace

enum yawc_server_error { OK = 0,
    FAILED_TO_START,
    FAILED_TO_CREATE_SOCKET,
    IS_NOT_GLES2
};

struct yawc_pointer {
public:
    struct wl_list link;
    struct wlr_input_device* wlr_device;

    struct wl_listener destroy;
};

struct yawc_keyboard {
public:
    struct wl_list link;
    struct yawc_server* server;
    struct wlr_keyboard* wlr_keyboard;

    struct wl_listener modifiers;
    struct wl_listener key;
    struct wl_listener destroy;
};

struct yawc_session_lock {
	struct wlr_session_lock_v1 *lock;
	struct wlr_surface *focused;
	bool crashed; //more understandable that way

	struct wl_list outputs;

	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
};

struct yawc_output {
public:
    struct wlr_output* wlr_output;
    struct yawc_server* server;
    struct timespec last_frame;

    int last_width, last_height;

    float last_scale;

    struct wl_list layer_surfaces;
    struct wl_list link;

    struct wl_listener destroy;
    struct wl_listener frame;
    struct wl_listener request_state;
    struct wl_listener commit;
};

struct yawc_input_on_surface {
    struct wlr_surface* surface;
    int x;
    int y;
};

struct yawc_input_on_node {
    int x;
    int y;
};

struct yawc_idle_inhibitor{
    struct wl_list link;
    
    wl_listener on_destroy;

    struct yawc_server *server;
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor;
};

enum yawc_mouse_operation { NOTHING,
    MOVING,
    RESIZING };

struct yawc_global_shortcut{
    struct wl_list link;
    struct wlr_hyprland_global_shortcut_v1 *shortcut; 

    struct wl_listener destroy;
};

struct yawc_xwayland_manager{
    int display_num;
    pid_t pid;

    struct wl_event_source *startup, 
                           *death;
};

struct yawc_keybind_manager{
    yawc_keybind_manager(struct yawc_server *server);

    struct yawc_server *server;

    struct yawc_global_shortcut *cur_global_shortcut;
    uint32_t cur_pressed_button;

    std::string global_shortcut_path;

    std::set<uint32_t> pressed_keys;

    struct yawc_bind_node *cur_node;
    time_t last_time;
    
    bool needed();
    bool in_sequence();
    void store(uint32_t button, uint32_t modifiers, bool pressed);
    struct yawc_bind_node *triggered();
    void execute_action(struct yawc_bind_node *node, uint32_t trigger);
    void reload();
};

struct yawc_wm_interface {
    void *handle; 
    
    wm_callbacks_t callbacks; 
    
    bool (*register_fn)(const wm_callbacks_t*, void*);
    void (*unregister_fn)();

    uint64_t hash;
};

struct yawc_server {
public:
    struct yawc_config *config;

    struct wl_display* wl_display;
    struct wl_event_loop* wl_event_loop;

    struct wlr_session* session;
    struct wlr_backend* backend;
    struct wlr_renderer* renderer;

    struct wlr_allocator* allocator, *shm_allocator;

    struct wlr_compositor *compositor;

    struct wlr_seat* seat;

    struct wlr_output_layout* output_layout;

    struct wlr_scene* scene = nullptr; 
    struct wlr_scene_output_layout* scene_layout;
    struct wlr_output_manager_v1 *output_manager;
    struct wlr_xdg_output_manager_v1 *xdg_output_manager;

    struct wlr_xdg_shell* xdg_shell;

    struct wlr_cursor* cursor = nullptr;
    struct wlr_xcursor_manager* cursor_mgr = nullptr;

    struct wlr_xdg_decoration_manager_v1 *decoration_manager;
    struct wlr_idle_inhibit_manager_v1 *idle_inhibit_manager;
    struct wlr_idle_notifier_v1 *idle_notify_manager;
    struct wlr_layer_shell_v1 *layer_shell;

    struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;

    struct wlr_pointer_constraints_v1 *pointer_constraints;
    struct wlr_relative_pointer_manager_v1 *relative_pointer_manager;
    struct wlr_pointer_constraint_v1 *active_constraint = nullptr;

    struct wlr_data_device_manager *data_device_manager;
    struct wlr_primary_selection_v1_device_manager *primary_selection_manager;

    struct wlr_gamma_control_manager_v1 *gamma_control_manager;

    struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1* ext_foreign_toplevel_image_capture_source_manager_v1;

    struct wlr_hyprland_global_shortcuts_manager_v1 *shortcuts_manager;

	struct wlr_linux_dmabuf_v1 *linux_dmabuf_v1;

    struct wl_event_source *reload_event;

    struct yawc_keybind_manager *keybind_manager;

    struct yawc_xwayland_manager xwayland_manager;

	struct {
		bool running;

		struct {
			struct wl_listener request;
			struct wl_listener start;
			struct wl_listener destroy;
		} events;

		struct wlr_scene_tree *icons;
	} drag;

    struct yawc_session_lock cur_lock;

	struct wlr_session_lock_manager_v1 *session_lock_manager;

    struct wlr_layer_surface_v1 *focused_layer;
    bool has_exclusive_layer;
    void set_focus_layer(struct wlr_layer_surface_v1 *layer);
    void set_focus_surface(struct wlr_surface *surface);

    struct wl_listener new_output_listener, new_xdg_toplevel_listener,
        xdg_toplevel_listener_destroy,
        seat_destroy,
        output_manager_destroy,
        decoration_manager_destroy,
        new_input_listener, pointer_motion_listener,
        pointer_motion_absolute_listener, cursor_frame_listener,
        cursor_button_listener, on_pointer_focus_change,
        on_request_cursor,
        on_request_set_selection,
        on_request_set_primary_selection,
        on_request_start_drag,
        on_axis_cursor_motion,
        on_new_idle_inhibitor,
        on_inhibitor_destroy,
        output_manager_test,
        output_manager_apply,
        backend_destroy,
        new_xdg_toplevel_decoration,
        new_layer_shell_surface,
        layer_shell_destroy,
        new_pointer_constraint,
        pointer_constraint_commit,
        pointer_constraint_destroy,
        pointer_constraint_manager_destroy,
        new_foreign_toplevel_capture_request,
        foreign_toplevel_capture_handler_destroy,
        new_session_lock,
        session_lock_manager_destroy,
        cursor_shape_set,
        new_shortcut;

    struct wl_list outputs, toplevels, decorations, keyboards, pointers, inhibitors, shortcuts;

    double grabbed_mov_x, grabbed_mov_y;
    double grabbed_res_x, grabbed_res_y;
    struct wlr_box grabbed_geo_box;
    uint32_t resize_edges;
    struct yawc_toplevel* grabbed_toplevel;
    enum yawc_mouse_operation current_mouse_operation;

    yawc_server();

    yawc_server_error run();

    yawc_server_error handle_outputs();

    void create_scene();
    void create_xdg_shell();
    void create_cursor();
    void create_pointer_constraint();

    void setup_seat();
    void setup_drag();
    void setup_screensharing();
    void setup_shortcuts();

    void create_idle_manager();
    void update_idle_inhibitors();

    void create_decoration_manager();

    void create_session_lock_manager();

    void create_layer_shell();

    void setup_xwayland();

    yawc_server_error setup_backend();

    void handle_new_output(struct wl_listener*, void*);

    void new_xdg_toplevel(struct wl_listener*, void*);

    yawc_keyboard *handle_keyboard(struct wlr_input_device *device);
    yawc_pointer *handle_pointer(struct wlr_input_device *device);
    void load_keyboard_cfg(yawc_keyboard *keyboard);
    void load_pointer_cfg(yawc_pointer *pointer);

    void handle_new_input(struct wl_listener*, void*);
    void handle_pointer_motion(struct wl_listener*, void*, bool);

    bool do_mouse_operation();

    void reset_cursor_mode();

    void constrain_cursor(struct wlr_pointer_constraint_v1 *constraint);
    void handle_pointer_motion_constraint(double &dx, double &dy);

    ~yawc_server();

    yawc_wm_interface wm;
    void load_wm(const char *path);
    void unload_wm();

    std::map<std::string, wlr_scene_buffer*> overlays;

	struct {
		struct wlr_scene_tree *background;
		struct wlr_scene_tree *bottom;
    	struct wlr_scene_tree *top;
        struct wlr_scene_tree *normal;
        struct wlr_scene_tree *fullscreen;
        struct wlr_scene_tree *unmanaged;
		struct wlr_scene_tree *overlay;
        struct wlr_scene_tree *screenlock;
	} layers;
    
};
