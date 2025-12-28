/*
#include <tuple>

#include "../utils.hpp"
#include "../wm_defs.hpp"

static struct {
    xcb_atom_t _NET_WM_WINDOW_TYPE;
    xcb_atom_t _NET_WM_WINDOW_TYPE_NORMAL;
    xcb_atom_t _NET_WM_WINDOW_TYPE_DOCK;
    xcb_atom_t _NET_WM_WINDOW_TYPE_TOOLBAR;
    xcb_atom_t _NET_WM_WINDOW_TYPE_SPLASH;
    xcb_atom_t _NET_WM_WINDOW_TYPE_MENU;
    xcb_atom_t _NET_WM_WINDOW_TYPE_DROPDOWN_MENU;
    xcb_atom_t _NET_WM_WINDOW_TYPE_POPUP_MENU;
    xcb_atom_t _NET_WM_WINDOW_TYPE_TOOLTIP;
    xcb_atom_t _NET_WM_WINDOW_TYPE_NOTIFICATION;
    xcb_atom_t _NET_WM_WINDOW_TYPE_COMBO;
    xcb_atom_t _NET_WM_WINDOW_TYPE_DND;
    xcb_atom_t _NET_WM_WINDOW_TYPE_DESKTOP;
} atoms;

void xtoplevel_unmap(yawc_toplevel *toplevel){
	toplevel->unmap();

	wl_list_remove(&toplevel->events.commit.link);

    if (toplevel->scene_tree) {
        wl_list_remove(&toplevel->xevents.scene_tree_destroy.link);

        if(toplevel->has_resize_grips){
            for(int i = 0; i < 8; ++i){
                scene_descriptor_destroy(&toplevel->resize_grips[i]->node);

                toplevel->resize_grips[i] = nullptr;
            }
            toplevel->has_resize_grips = false;
        }

        scene_descriptor_destroy(&toplevel->scene_tree->node);
        wlr_scene_node_destroy(&toplevel->scene_tree->node);

        toplevel->scene_tree = nullptr;
        toplevel->foreign_on_output_handler = nullptr;
    }

	if(toplevel->decoration){
		delete toplevel->decoration;
        toplevel->decoration = nullptr;
	}
}

void xtoplevel_destroy(struct yawc_toplevel *toplevel){
    if (toplevel->mapped) {
        xtoplevel_unmap(toplevel);
    }

	wl_list_remove(&toplevel->events.destroy.link);
	wl_list_remove(&toplevel->events.request_fullscreen.link);
    wl_list_remove(&toplevel->events.request_maximize.link);
	wl_list_remove(&toplevel->events.request_minimize.link);
	wl_list_remove(&toplevel->events.request_move.link);
	wl_list_remove(&toplevel->events.request_resize.link);

    wl_list_remove(&toplevel->xevents.request_close.link);
	wl_list_remove(&toplevel->xevents.request_activate.link);
	wl_list_remove(&toplevel->xevents.request_configure.link);
	wl_list_remove(&toplevel->xevents.set_title.link);
	wl_list_remove(&toplevel->xevents.set_app_id.link);
	wl_list_remove(&toplevel->xevents.set_decorations.link);
	wl_list_remove(&toplevel->xevents.associate.link);
	wl_list_remove(&toplevel->xevents.dissociate.link);
	wl_list_remove(&toplevel->xevents.override_redirect.link);

    delete toplevel;
}

void handle_xtoplevel_commit(struct wl_listener *listener, void *data){
    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, events.commit);
    
    toplevel->commit();
}

void handle_scene_tree_destroy(struct wl_listener *listener, void *data){
    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, xevents.scene_tree_destroy);
    
    toplevel->scene_tree = nullptr;
    toplevel->foreign_on_output_handler = nullptr;
}

void handle_xtoplevel_map(struct wl_listener *listener, void *data){
    struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, events.map);
    auto *xsurface = toplevel->xwayland_surface; 

 	toplevel->scene_tree = wlr_scene_subsurface_tree_create(toplevel->server->layers.normal, xsurface->surface);

    if (!scene_descriptor_assign(&toplevel->scene_tree->node,
            YAWC_SCENE_DESC_VIEW, toplevel, 0)) {
        wlr_log(WLR_ERROR, "Failed to assign scene descriptor to toplevel");
        wlr_scene_node_destroy(&toplevel->scene_tree->node);
        delete toplevel;
		return;
    }

	if (toplevel->scene_tree) {
		toplevel->xevents.scene_tree_destroy.notify = handle_scene_tree_destroy;
		wl_signal_add(&toplevel->scene_tree->node.events.destroy, &toplevel->xevents.scene_tree_destroy);
	}

    toplevel->setup_foreign_output_handler();

    xsurface->surface->data = toplevel->scene_tree;

    toplevel->events.commit.notify = handle_xtoplevel_commit;
    wl_signal_add(&xsurface->surface->events.commit, &toplevel->events.commit);

    if(!toplevel->decoration){
        toplevel->decoration = new yawc_toplevel_decoration{}; //we create the decoration manually, X doesn't send us an event
    } 

    toplevel->decoration->toplevel = toplevel;
    toplevel->decoration->xdg_decoration = nullptr;
    toplevel->decoration->width = 0;
    toplevel->decoration->is_ssd = xsurface->decorations == WLR_XWAYLAND_SURFACE_DECORATIONS_ALL;
	
    toplevel->map();
}

void handle_xtoplevel_unmap(struct wl_listener *listener, void *data){
    struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, events.unmap);

    xtoplevel_unmap(toplevel);
}

void handle_xtoplevel_associate(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, xevents.associate);

    struct wlr_xwayland_surface *xsurface =
		toplevel->xwayland_surface;

    toplevel->events.map.notify = handle_xtoplevel_map;
    toplevel->events.unmap.notify = handle_xtoplevel_unmap;
    wl_signal_add(&xsurface->surface->events.map, &toplevel->events.map);
    wl_signal_add(&xsurface->surface->events.unmap, &toplevel->events.unmap);
}

void xtoplevel_dissociate(struct yawc_toplevel *toplevel){
    wl_list_remove(&toplevel->events.map.link);
    wl_list_remove(&toplevel->events.unmap.link);
}

void handle_xtoplevel_dissociate(struct wl_listener *listener, void *data){
    struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, xevents.dissociate);

    xtoplevel_dissociate(toplevel);
}

void handle_xtoplevel_request_configure(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, xevents.request_configure);

	struct wlr_xwayland_surface *xsurface = toplevel->xwayland_surface;
	struct wlr_xwayland_surface_configure_event *ev = reinterpret_cast<struct wlr_xwayland_surface_configure_event *>(data);

	wlr_xwayland_surface_configure(xsurface, ev->x, ev->y, ev->width, ev->height);
}

void handle_xtoplevel_request_fullscreen(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, events.request_fullscreen);

    if(!toplevel->mapped){
        return;
    }

	struct wlr_xwayland_surface *xsurface = toplevel->xwayland_surface;

	toplevel->request_fullscreen(xsurface->fullscreen);
}

void handle_xtoplevel_request_minimize(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, events.request_minimize);

    if(!toplevel->mapped){
        return;
    }

    struct wlr_xwayland_minimize_event *ev = 
        static_cast<struct wlr_xwayland_minimize_event*>(data);
	
	toplevel->request_minimize(ev->minimize);
}

void handle_xtoplevel_request_maximize(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, events.request_maximize);

    if(!toplevel->mapped){
        return;
    }

	struct wlr_xwayland_surface *xsurface = toplevel->xwayland_surface;
	
	toplevel->request_maximize(xsurface->maximized_horz && xsurface->maximized_vert);
}

void handle_xtoplevel_request_activate(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, xevents.request_activate);

    if(!toplevel->mapped){
        return;
    }

    toplevel->request_activate();
}

void handle_xtoplevel_request_move(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, events.request_move);

    if(!toplevel->mapped){
        return;
    }
	
    toplevel->request_move();
}

void handle_xtoplevel_request_resize(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, events.request_resize);

    if(!toplevel->mapped){
        return;
    }

    struct wlr_xwayland_resize_event *ev = reinterpret_cast<struct wlr_xwayland_resize_event*>(data);

    toplevel->request_resize(ev->edges);
}

void handle_xtoplevel_request_close(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, xevents.request_close);

    if(!toplevel->mapped){
        return;
    }

    toplevel->request_close();
}

void handle_xtoplevel_set_title(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, xevents.set_title);

	struct wlr_xwayland_surface *xsurface = toplevel->xwayland_surface;

    if(!toplevel->mapped){
        return;
    }

    if(!xsurface){
        return;
    }

	toplevel->set_title(xsurface->title);
}

void handle_xtoplevel_set_app_id(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, xevents.set_app_id);

    if(!toplevel->mapped){
        return;
    }

	struct wlr_xwayland_surface *xsurface = toplevel->xwayland_surface;

    if(!xsurface){
        return;
    }

	toplevel->set_app_id(xsurface->class_);
}

void handle_xtoplevel_set_decorations(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, xevents.set_decorations);
	struct wlr_xwayland_surface *xsurface = toplevel->xwayland_surface;

	if(!toplevel->decoration){
		return;
	}

	toplevel->decoration->is_ssd = xsurface->decorations == WLR_XWAYLAND_SURFACE_DECORATIONS_ALL;
}

void handle_xtoplevel_destroy(struct wl_listener *listener, void *data){
	struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, events.destroy);

    xtoplevel_destroy(toplevel);
}

void handle_xtoplevel_override_redirect(struct wl_listener *listener, void *data) {
    struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, xevents.override_redirect);

    auto *server = toplevel->server;
    auto *xsurface = toplevel->xwayland_surface;
    
	bool associated = xsurface->surface != NULL;
	bool mapped = associated && toplevel->mapped;

	if (mapped) {
        xtoplevel_unmap(toplevel);
	}

	if (associated) {
        xtoplevel_dissociate(toplevel);
	}

	xtoplevel_destroy(toplevel);
    xsurface->data = NULL;

    yawc_toplevel_unmanaged *unmanaged = create_toplevel_xwl_unmanaged(server, xsurface);

    server->setup_xwayland_toplevel_unmanaged(unmanaged, xsurface, associated, mapped);
}

bool is_unmanaged(yawc_server *server, struct wlr_xwayland_surface *xsurface) {
    if (!xsurface->window_type_len) {
        return false;
    }

    if (xsurface->width <= 1 && xsurface->height <= 1) {
        return true;
    }

    for (size_t i = 0; i < xsurface->window_type_len; i++) {
        xcb_atom_t type = xsurface->window_type[i];

        if (type == atoms._NET_WM_WINDOW_TYPE_DOCK ||
            type == atoms._NET_WM_WINDOW_TYPE_SPLASH ||
            type == atoms._NET_WM_WINDOW_TYPE_TOOLBAR ||
            type == atoms._NET_WM_WINDOW_TYPE_MENU ||
            type == atoms._NET_WM_WINDOW_TYPE_DROPDOWN_MENU ||
            type == atoms._NET_WM_WINDOW_TYPE_POPUP_MENU ||
            type == atoms._NET_WM_WINDOW_TYPE_TOOLTIP ||
            type == atoms._NET_WM_WINDOW_TYPE_NOTIFICATION ||
            type == atoms._NET_WM_WINDOW_TYPE_COMBO ||
            type == atoms._NET_WM_WINDOW_TYPE_DND  ||
            type == atoms._NET_WM_WINDOW_TYPE_DESKTOP

            ) {
            return true;
        }
    }

    return false;
}

void handle_new_surface(struct wl_listener *listener, void *data){
    yawc_server *server = wl_container_of(listener, server, xwayland_new_surface);

    auto *xsurface = reinterpret_cast<struct wlr_xwayland_surface*>(data);

    if (is_unmanaged(server, xsurface) || xsurface->override_redirect) {
        yawc_toplevel_unmanaged *toplevel = create_toplevel_xwl_unmanaged(server, xsurface);

        server->setup_xwayland_toplevel_unmanaged(toplevel, xsurface, false, false);

        return;
    }

    yawc_toplevel *toplevel = create_toplevel_xwl(server, xsurface);

    server->setup_xwayland_toplevel(toplevel, xsurface, false, false); 
}

void handle_surface_destroy(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, xwayland_surface_destroy);

    wl_list_remove(&server->xwayland_new_surface.link);
    wl_list_remove(&server->xwayland_surface_destroy.link);
	wl_list_remove(&server->xwayland_ready.link);
}

void yawc_server::setup_xwayland_toplevel(struct yawc_toplevel* toplevel, struct wlr_xwayland_surface *xsurface, bool associate, bool map){
    xsurface->data = toplevel;

	toplevel->events.destroy.notify = handle_xtoplevel_destroy;
    wl_signal_add(&xsurface->events.destroy, &toplevel->events.destroy);

	toplevel->xevents.request_configure.notify = handle_xtoplevel_request_configure;
    wl_signal_add(&xsurface->events.request_configure, &toplevel->xevents.request_configure);

	toplevel->events.request_maximize.notify = handle_xtoplevel_request_maximize;
	wl_signal_add(&xsurface->events.request_maximize, &toplevel->events.request_maximize);

	toplevel->events.request_fullscreen.notify = handle_xtoplevel_request_fullscreen;
	wl_signal_add(&xsurface->events.request_fullscreen, &toplevel->events.request_fullscreen);

	toplevel->events.request_minimize.notify = handle_xtoplevel_request_minimize;
	wl_signal_add(&xsurface->events.request_minimize, &toplevel->events.request_minimize);
	
	toplevel->xevents.request_activate.notify = handle_xtoplevel_request_activate;
	wl_signal_add(&xsurface->events.request_activate, &toplevel->xevents.request_activate);

	toplevel->events.request_move.notify = handle_xtoplevel_request_move;
    wl_signal_add(&xsurface->events.request_move, &toplevel->events.request_move);

	toplevel->events.request_resize.notify = handle_xtoplevel_request_resize;
    wl_signal_add(&xsurface->events.request_resize, &toplevel->events.request_resize);

	toplevel->xevents.request_close.notify = handle_xtoplevel_request_close;
    wl_signal_add(&xsurface->events.request_close, &toplevel->xevents.request_close);

	toplevel->xevents.set_title.notify = handle_xtoplevel_set_title;
	wl_signal_add(&xsurface->events.set_title, &toplevel->xevents.set_title);

	toplevel->xevents.set_app_id.notify = handle_xtoplevel_set_app_id;
	wl_signal_add(&xsurface->events.set_class, &toplevel->xevents.set_app_id);

	toplevel->xevents.set_decorations.notify = handle_xtoplevel_set_decorations;
	wl_signal_add(&xsurface->events.set_decorations, &toplevel->xevents.set_decorations);

    toplevel->xevents.override_redirect.notify = handle_xtoplevel_override_redirect;
	wl_signal_add(&xsurface->events.set_override_redirect, &toplevel->xevents.override_redirect);

    toplevel->xevents.associate.notify = handle_xtoplevel_associate;
    wl_signal_add(&xsurface->events.associate, &toplevel->xevents.associate);

    toplevel->xevents.dissociate.notify = handle_xtoplevel_dissociate;
    wl_signal_add(&xsurface->events.dissociate, &toplevel->xevents.dissociate);

    if(associate){
        handle_xtoplevel_associate(&toplevel->xevents.associate, NULL);
    }

    if(map){
     	handle_xtoplevel_map(&toplevel->events.map, NULL);
    }
}

void handle_xwayland_ready(struct wl_listener *listener, void *data){
    yawc_server *server = wl_container_of(listener, server, xwayland_ready);

    wlr_xwayland_set_seat(server->xwayland, server->seat);

	xcb_connection_t *xcb_conn = xcb_connect(NULL, NULL);
	int err = xcb_connection_has_error(xcb_conn);

	if (err) {
        wlr_log(WLR_ERROR, "Failed to connect with XCB %d", err);
		return;
	}

    auto get_atom = [&](const char* name) -> xcb_atom_t {
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xcb_conn, 0, strlen(name), name);
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(xcb_conn, cookie, NULL);
        xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
        free(reply);
        return atom;
    };

    atoms._NET_WM_WINDOW_TYPE = get_atom("_NET_WM_WINDOW_TYPE");
    atoms._NET_WM_WINDOW_TYPE_NORMAL = get_atom("_NET_WM_WINDOW_TYPE_NORMAL");
    atoms._NET_WM_WINDOW_TYPE_DOCK = get_atom("_NET_WM_WINDOW_TYPE_DOCK");
    atoms._NET_WM_WINDOW_TYPE_TOOLBAR = get_atom("_NET_WM_WINDOW_TYPE_TOOLBAR");
    atoms._NET_WM_WINDOW_TYPE_SPLASH = get_atom("_NET_WM_WINDOW_TYPE_SPLASH");
    atoms._NET_WM_WINDOW_TYPE_MENU = get_atom("_NET_WM_WINDOW_TYPE_MENU");
    atoms._NET_WM_WINDOW_TYPE_DROPDOWN_MENU = get_atom("_NET_WM_WINDOW_TYPE_DROPDOWN_MENU");
    atoms._NET_WM_WINDOW_TYPE_POPUP_MENU = get_atom("_NET_WM_WINDOW_TYPE_POPUP_MENU");
    atoms._NET_WM_WINDOW_TYPE_TOOLTIP = get_atom("_NET_WM_WINDOW_TYPE_TOOLTIP");
    atoms._NET_WM_WINDOW_TYPE_NOTIFICATION = get_atom("_NET_WM_WINDOW_TYPE_NOTIFICATION");
    atoms._NET_WM_WINDOW_TYPE_COMBO = get_atom("_NET_WM_WINDOW_TYPE_COMBO");
    atoms._NET_WM_WINDOW_TYPE_DND = get_atom("_NET_WM_WINDOW_TYPE_DND");
    atoms._NET_WM_WINDOW_TYPE_DESKTOP = get_atom("_NET_WM_WINDOW_TYPE_DESKTOP");

    xcb_disconnect(xcb_conn);
}

void yawc_server::setup_xwayland(){
	this->xwayland = wlr_xwayland_create(this->wl_display, this->compositor, true);

	this->xwayland_ready.notify = handle_xwayland_ready;    
    wl_signal_add(&this->xwayland->events.ready, &this->xwayland_ready);

    this->xwayland_new_surface.notify = handle_new_surface;
    wl_signal_add(&this->xwayland->events.new_surface, &this->xwayland_new_surface);

    this->xwayland_surface_destroy.notify = handle_surface_destroy;
    wl_signal_add(&this->xwayland->events.destroy, &this->xwayland_surface_destroy);
}*/
