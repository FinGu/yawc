#include "../utils.hpp"
#include "../toplevel.hpp"

void toplevel_decoration_destroy(struct wl_listener *listener, void *data){
    struct yawc_toplevel_decoration *decoration = wl_container_of(listener, decoration, destroy);

    wl_list_remove(&decoration->destroy.link);
    wl_list_remove(&decoration->request_mode.link);

    delete decoration;
}

void toplevel_decoration_request_mode(struct wl_listener *listener, void *data){
    struct yawc_toplevel_decoration *decoration = wl_container_of(listener, decoration, request_mode);

    auto req_mode = decoration->xdg_decoration->requested_mode;

    auto new_mode = req_mode;
    if (req_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE) {
        new_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    }

    decoration->is_ssd = (new_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);

    if(decoration->toplevel->xdg_toplevel->base->initialized){
        wlr_xdg_toplevel_decoration_v1_set_mode(decoration->xdg_decoration, new_mode);
    }
}

void setup_decoration(struct yawc_toplevel_decoration *decoration){
    decoration->request_mode.notify = toplevel_decoration_request_mode;
    wl_signal_add(&decoration->xdg_decoration->events.request_mode, &decoration->request_mode);

    decoration->destroy.notify = toplevel_decoration_destroy;
    wl_signal_add(&decoration->xdg_decoration->events.destroy, &decoration->destroy);

    toplevel_decoration_request_mode(&decoration->request_mode, nullptr);
}

void new_toplevel_decoration(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, new_xdg_toplevel_decoration);
    struct wlr_xdg_toplevel_decoration_v1 *xdg_decoration = reinterpret_cast<struct wlr_xdg_toplevel_decoration_v1*>(data);

    yawc_toplevel *toplevel;

    wl_list_for_each(toplevel, &server->toplevels, link){
        if(toplevel->xdg_toplevel != xdg_decoration->toplevel){
            continue;
        }

        break;
    }

    wlr_log(WLR_DEBUG, "Making a decoration for the toplevel");

    auto *decoration = new yawc_toplevel_decoration{};
    decoration->toplevel = toplevel;
    decoration->xdg_decoration = xdg_decoration;
        
    toplevel->decoration = decoration;

    setup_decoration(decoration);
}

void on_decoration_manager_destroy(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, decoration_manager_destroy);

    wl_list_remove(&server->new_xdg_toplevel_decoration.link);
    wl_list_remove(&server->decoration_manager_destroy.link);
}

void yawc_server::create_decoration_manager(){
    this->decoration_manager = wlr_xdg_decoration_manager_v1_create(this->wl_display);

    this->new_xdg_toplevel_decoration.notify = new_toplevel_decoration;
    wl_signal_add(&this->decoration_manager->events.new_toplevel_decoration, &this->new_xdg_toplevel_decoration);

    this->decoration_manager_destroy.notify = on_decoration_manager_destroy;
    wl_signal_add(&this->decoration_manager->events.destroy, &this->decoration_manager_destroy);
}

