struct yawc_toplevel_decoration;

void toplevel_decoration_destroy(struct wl_listener *listener, void *data);

void toplevel_decoration_request_mode(struct wl_listener *listener, void *data);

void setup_decoration(struct yawc_toplevel_decoration *decoration);

