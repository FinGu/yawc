/*
#include <tuple>

#include "../utils.hpp"
#include "../wm_defs.hpp"

void handle_unmanaged_request_configure(struct wl_listener *listener, void *data) {
    struct yawc_toplevel_unmanaged *surface = wl_container_of(listener, surface, request_configure);
    struct wlr_xwayland_surface_configure_event *ev = (struct wlr_xwayland_surface_configure_event *)data;
    
    wlr_xwayland_surface_configure(surface->xsurface, ev->x, ev->y, ev->width, ev->height);
}

void handle_unmanaged_request_activate(struct wl_listener *listener, void *data) {
    struct yawc_toplevel_unmanaged *surface = wl_container_of(listener, surface, request_activate);

	wlr_xwayland_surface_set_minimized(surface->xsurface, false);
	wlr_xwayland_surface_activate(surface->xsurface, true);

    surface->server->set_focus_surface(surface->xsurface->surface);
}

void handle_unmanaged_set_geometry(struct wl_listener *listener, void *data) {
    struct yawc_toplevel_unmanaged *surface = wl_container_of(listener, surface, set_geometry);
    
    wlr_scene_node_set_position(&surface->scene_tree->node, surface->xsurface->x, surface->xsurface->y);
}

void handle_unmanaged_map(struct wl_listener *listener, void *data) {
    struct yawc_toplevel_unmanaged *xsurface = wl_container_of(listener, xsurface, map);
    
    xsurface->scene_tree = wlr_scene_subsurface_tree_create(
        xsurface->server->layers.unmanaged, xsurface->xsurface->surface);
    
    wlr_scene_node_set_position(&xsurface->scene_tree->node, xsurface->xsurface->x, xsurface->xsurface->y);

    xsurface->set_geometry.notify = handle_unmanaged_set_geometry;
    wl_signal_add(&xsurface->xsurface->events.set_geometry, &xsurface->set_geometry);

    if (wlr_xwayland_surface_override_redirect_wants_focus(xsurface->xsurface)) { //we're not gonna talk about this
        xsurface->server->set_focus_layer(nullptr);
        xsurface->server->set_focus_surface(xsurface->xsurface->surface);
    }

    xsurface->mapped = true;
}

void unmanaged_unmap(struct yawc_toplevel_unmanaged *toplevel){
    wl_list_remove(&toplevel->set_geometry.link);

    if (toplevel->scene_tree) {
        wlr_scene_node_destroy(&toplevel->scene_tree->node);
        toplevel->scene_tree = nullptr;
    }

    toplevel->mapped = false;
}

void handle_unmanaged_unmap(struct wl_listener *listener, void *data) {
    struct yawc_toplevel_unmanaged *surface = wl_container_of(listener, surface, unmap);
    unmanaged_unmap(surface);
}

void unmanaged_dissociate(struct yawc_toplevel_unmanaged *toplevel){
    wl_list_remove(&toplevel->map.link);
    wl_list_remove(&toplevel->unmap.link);
}

void handle_unmanaged_dissociate(struct wl_listener *listener, void *data) {
    struct yawc_toplevel_unmanaged *xsurface = wl_container_of(listener, xsurface, dissociate);

    unmanaged_dissociate(xsurface);
}


void unmanaged_destroy(struct yawc_toplevel_unmanaged *toplevel) {
    if (toplevel->mapped) {
        unmanaged_unmap(toplevel);
    }

    if (toplevel->xsurface->surface) {
        unmanaged_dissociate(toplevel);
    }

    wl_list_remove(&toplevel->destroy.link);
    wl_list_remove(&toplevel->request_configure.link);
    wl_list_remove(&toplevel->request_activate.link);
    wl_list_remove(&toplevel->associate.link);
    wl_list_remove(&toplevel->dissociate.link);
    wl_list_remove(&toplevel->override_redirect.link);

    delete toplevel;
}

void handle_unmanaged_destroy(struct wl_listener *listener, void *data) {
    struct yawc_toplevel_unmanaged *surface = wl_container_of(listener, surface, destroy);
    
    unmanaged_destroy(surface);
}

void handle_unmanaged_override_redirect(struct wl_listener *listener, void *data) {
    struct yawc_toplevel_unmanaged *unmanaged = wl_container_of(listener, unmanaged, override_redirect);

    auto *server = unmanaged->server;
    auto *xsurface = unmanaged->xsurface;
    
	bool associated = xsurface->surface != NULL;
	bool mapped = associated && unmanaged->mapped;

	if (mapped) {
		unmanaged_unmap(unmanaged);
	}

	if (associated) {
		unmanaged_dissociate(unmanaged);
	}

	unmanaged_destroy(unmanaged);
	xsurface->data = NULL;

    yawc_toplevel *toplevel = create_toplevel_xwl(server, xsurface);

    server->setup_xwayland_toplevel(toplevel, xsurface, associated, mapped);
}

void handle_unmanaged_associate(struct wl_listener *listener, void *data) {
    struct yawc_toplevel_unmanaged *xsurface = wl_container_of(listener, xsurface, associate);
    
    xsurface->map.notify = handle_unmanaged_map;
    wl_signal_add(&xsurface->xsurface->surface->events.map, &xsurface->map);
    
    xsurface->unmap.notify = handle_unmanaged_unmap;
    wl_signal_add(&xsurface->xsurface->surface->events.unmap, &xsurface->unmap);
}

void yawc_server::setup_xwayland_toplevel_unmanaged(yawc_toplevel_unmanaged *toplevel, struct wlr_xwayland_surface *xsurface,
        bool associate, bool map) {
    toplevel->destroy.notify = handle_unmanaged_destroy;
    wl_signal_add(&xsurface->events.destroy, &toplevel->destroy);
    
    toplevel->request_configure.notify = handle_unmanaged_request_configure;
    wl_signal_add(&xsurface->events.request_configure, &toplevel->request_configure);

    toplevel->associate.notify = handle_unmanaged_associate;
    wl_signal_add(&xsurface->events.associate, &toplevel->associate);
    
    toplevel->dissociate.notify = handle_unmanaged_dissociate;
    wl_signal_add(&xsurface->events.dissociate, &toplevel->dissociate);

    toplevel->override_redirect.notify = handle_unmanaged_override_redirect;
    wl_signal_add(&xsurface->events.set_override_redirect, &toplevel->override_redirect);

    toplevel->request_activate.notify = handle_unmanaged_request_activate;
    wl_signal_add(&xsurface->events.request_activate, &toplevel->request_activate);

}
*/
