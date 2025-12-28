#include "server.hpp"

struct yawc_toplevel_geometry{
    int x, y, width, height;
};

struct yawc_toplevel_decoration{
    struct yawc_toplevel *toplevel;
    struct wlr_xdg_toplevel_decoration_v1 *xdg_decoration;

    struct wlr_scene_buffer *ssd_scene_buffer;

    int width;

    struct wl_listener pointer_motion; 
    struct wl_listener pointer_button; 

    struct wl_listener request_mode;
    struct wl_listener destroy;

    bool is_ssd;
};

struct yawc_toplevel {
    struct wl_list link;
    struct yawc_server* server;

    struct wlr_xdg_toplevel *xdg_toplevel;

    struct yawc_toplevel_decoration *decoration;

    void *wm_state;

    uint64_t id;

    struct yawc_toplevel_geometry last_geo;

    bool hidden : 1;
    bool maximized : 1;
    bool mapped : 1;
    bool fullscreen : 1;

    struct wlr_foreign_toplevel_handle_v1 *foreign_handle;
    struct wlr_scene_buffer *foreign_on_output_handler; // ...

    struct wl_listener foreign_request_minimize, 
                       foreign_request_activate, 
                       foreign_request_close,
                       foreign_output_enter,
                       foreign_output_leave,
                       foreign_output_handler_destroy;

    void setup_foreign_output_handler();

    //top, bottom, left, right, top-left, top-right, bottom-left, bottom-right
    struct wlr_scene_rect *resize_grips[8];
    bool has_resize_grips : 1;

    struct wlr_scene *image_capture_scene;
    struct wlr_scene_surface *image_capture_scene_surface;
	struct wlr_ext_image_capture_source_v1 *image_capture_source;

    void send_geometry_update();

    void activate(bool yes);
    
    //events
    void map();
    void unmap();
    void commit();

    void request_maximize(bool enable);
    void request_minimize(bool enable);
    void request_fullscreen(bool enable);
    void request_move();
    void request_resize(uint32_t edges);

    void request_activate();
    void request_close();

    //default impls
    void set_fullscreen(bool enable);
    void default_set_minimized(bool enable);
    void default_set_maximized(bool enable);

    std::string title = "default", app_id = "default";
    void set_title(const char *title);
    void set_app_id(const char *app_id);

    struct yawc_toplevel_geometry reset_state();
    struct yawc_toplevel_geometry save_state();

    struct wlr_scene_tree* scene_tree;

    struct {
        struct wl_listener map;
        struct wl_listener unmap;
        struct wl_listener commit;
        struct wl_listener destroy;
        struct wl_listener request_move;
        struct wl_listener request_resize;
        struct wl_listener request_maximize;
        struct wl_listener request_minimize;
        struct wl_listener request_fullscreen;
        struct wl_listener new_popup;
    } events;

    ~yawc_toplevel();
};

struct yawc_toplevel_unmanaged {
    struct yawc_server *server;
    struct wlr_xwayland_surface *xsurface;
    struct wlr_scene_tree *scene_tree;

    bool mapped : 1;

    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener request_configure;
    struct wl_listener set_geometry;
    struct wl_listener associate;
    struct wl_listener dissociate;
    struct wl_listener override_redirect;

    struct wl_listener request_activate;
};

yawc_toplevel *create_toplevel_xdg(yawc_server *server, struct wlr_xdg_toplevel *xdg_toplevel);

struct yawc_popup {
    struct yawc_toplevel *toplevel;
	struct wlr_xdg_popup *xdg_popup;

	struct wlr_scene_tree *scene_tree;

	struct wl_listener commit;
	struct wl_listener new_popup;
	struct wl_listener reposition;
	struct wl_listener destroy;
};


