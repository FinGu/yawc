#include "server.hpp"
#include "../layer.hpp"
#include <wlr-layer-shell-unstable-v1-protocol.h>

struct wlr_scene_tree *layer_get_scene(struct yawc_server *sv, enum zwlr_layer_shell_v1_layer type) {
	switch (type) {
	case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
		return sv->layers.background;
	case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
		return sv->layers.bottom;
	case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
		return sv->layers.top;
	case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
		return sv->layers.overlay;
	}

	return nullptr;
}

void popup_handle_destroy(struct wl_listener *listener, void *data) {
    struct yawc_layer_popup *popup = wl_container_of(listener, popup, destroy);

    wl_list_remove(&popup->reposition.link);
    wl_list_remove(&popup->destroy.link);
    wl_list_remove(&popup->new_popup.link);
    wl_list_remove(&popup->commit.link);


    delete popup;
}

void popup_unconstrain(struct yawc_layer_popup *popup) {
    struct wlr_xdg_popup *wlr_popup = popup->wlr_popup;
    struct yawc_output *output = popup->toplevel->output;

    if (!output) {
        return;
    }

    int lx, ly;
    wlr_scene_node_coords(&popup->toplevel->scene->tree->node, &lx, &ly);

    struct wlr_box output_box;
    wlr_output_layout_get_box(output->server->output_layout, output->wlr_output, &output_box);

    struct wlr_box output_toplevel_sx_box = {
        .x = output_box.x - lx,
        .y = output_box.y - ly,
        .width = output_box.width,
        .height = output_box.height,
    };

    wlr_xdg_popup_unconstrain_from_box(wlr_popup, &output_toplevel_sx_box);
}

void popup_handle_commit(struct wl_listener *listener, void *data) {
    struct yawc_layer_popup *popup = wl_container_of(listener, popup, commit);
    if (popup->wlr_popup->base->initial_commit) {
        popup_unconstrain(popup);
    }
}

void popup_handle_reposition(struct wl_listener *listener, void *data) {
    struct yawc_layer_popup *popup = wl_container_of(listener, popup, reposition);

    popup_unconstrain(popup);
}

void lpopup_handle_new_popup(struct wl_listener *listener, void *data); 

struct yawc_layer_popup *create_layer_popup(struct wlr_xdg_popup *wlr_popup,
        struct yawc_layer_surface *toplevel, struct wlr_scene_tree *parent) {
    struct yawc_layer_popup *popup = new yawc_layer_popup{};
    if (!popup) {
        return nullptr;
    }

    popup->toplevel = toplevel;
    popup->wlr_popup = wlr_popup;
    popup->scene = wlr_scene_xdg_surface_create(parent, wlr_popup->base);

    if (!popup->scene) {
        delete popup;
        return nullptr;
    }

    popup->destroy.notify = popup_handle_destroy;
    wl_signal_add(&wlr_popup->events.destroy, &popup->destroy);

    popup->new_popup.notify = lpopup_handle_new_popup;
    wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);

    popup->commit.notify = popup_handle_commit;
    wl_signal_add(&wlr_popup->base->surface->events.commit, &popup->commit);

    popup->reposition.notify = popup_handle_reposition;
    wl_signal_add(&wlr_popup->events.reposition, &popup->reposition);

    return popup;
}

void lpopup_handle_new_popup(struct wl_listener *listener, void *data) {
    struct yawc_layer_popup *layer_popup = wl_container_of(listener, layer_popup, new_popup);
    struct wlr_xdg_popup *wlr_popup = reinterpret_cast<struct wlr_xdg_popup*>(data);

    create_layer_popup(wlr_popup, layer_popup->toplevel, layer_popup->scene);
}

void arrange_surface(struct yawc_output *output, const struct wlr_box *full_area,
		struct wlr_box *usable_area, struct wlr_scene_tree *tree, bool exclusive) {
	struct wlr_scene_node *node, *tmp;
	wl_list_for_each_safe(node, tmp, &tree->children, link) {
		struct yawc_layer_surface *surface = scene_descriptor_try_get(node,
			YAWC_SCENE_DESC_LAYER_SHELL);

		if (!surface) {
			continue;
		}

        if(surface->layer_surface->output != output->wlr_output){
            continue;
        }

		if (!surface->scene->layer_surface->initialized) {
			continue;
		}

		if ((surface->scene->layer_surface->current.exclusive_zone > 0) != exclusive) {
			continue;
		}

		wlr_scene_layer_surface_v1_configure(surface->scene, full_area, usable_area);
	}
}

void arrange_layers(struct yawc_output *output) {
    auto *server = output->server;

	struct wlr_box usable_area = { 0 };
	wlr_output_effective_resolution(output->wlr_output,
			&usable_area.width, &usable_area.height);
	const struct wlr_box full_area = usable_area;

	arrange_surface(output, &full_area, &usable_area, server->layers.overlay, true);
	arrange_surface(output, &full_area, &usable_area, server->layers.top, true);
	arrange_surface(output, &full_area, &usable_area, server->layers.bottom, true);
	arrange_surface(output, &full_area, &usable_area, server->layers.background, true);

	arrange_surface(output, &full_area, &usable_area, server->layers.overlay, false);
	arrange_surface(output, &full_area, &usable_area, server->layers.top, false);
	arrange_surface(output, &full_area, &usable_area, server->layers.bottom, false);
	arrange_surface(output, &full_area, &usable_area, server->layers.background, false);

	struct wlr_scene_tree *layers_above_shell[] = {
		server->layers.overlay,
		server->layers.top,
	};

	size_t nlayers = sizeof(layers_above_shell) / sizeof(layers_above_shell[0]);
	struct wlr_scene_node *node;
	struct yawc_layer_surface *topmost = nullptr;
	for (size_t i = 0; i < nlayers; ++i) {
		wl_list_for_each_reverse(node,
				&layers_above_shell[i]->children, link) {
			struct yawc_layer_surface *surface = scene_descriptor_try_get(node,
				YAWC_SCENE_DESC_LAYER_SHELL);
			if (surface && 
                    surface->layer_surface->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE && 
                    surface->layer_surface->surface->mapped) {
				topmost = surface;
				break;
			}
		}
		if (topmost != nullptr) {
			break;
		}
	}
	
	if(topmost){
		output->server->set_focus_layer(topmost->scene->layer_surface);
	}else{
		output->server->set_focus_layer(nullptr);
	}
}

void handle_layer_map(struct wl_listener *listener, void *data){
	struct yawc_layer_surface *surface = wl_container_of(listener,
			surface, map);

	struct wlr_layer_surface_v1 *layer_surface =
				surface->scene->layer_surface;

	if (layer_surface->current.keyboard_interactive &&
			(layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY ||
			layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP)) {

		struct yawc_server *server = surface->output->server;
		/* but only if the currently focused layer has a lower precedence */
		if (!server->focused_layer ||
				server->focused_layer->current.layer >= layer_surface->current.layer) {
			server->set_focus_layer(layer_surface);
		}
		arrange_layers(surface->output);
	}
}

void handle_layer_surface_commit(struct wl_listener *listener, void *data) {
    struct yawc_layer_surface *surface = wl_container_of(listener, surface, surface_commit);

    auto *server = surface->output->server;
    
    struct wlr_layer_surface_v1 *layer_surface = surface->scene->layer_surface;
    uint32_t committed = layer_surface->current.committed;
    
    if (layer_surface->initialized && committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
        enum zwlr_layer_shell_v1_layer layer_type = layer_surface->current.layer;
        struct wlr_scene_tree *output_layer = layer_get_scene(server, layer_type);
        wlr_scene_node_reparent(&surface->scene->tree->node, output_layer);
    }

    if (layer_surface->initial_commit || committed || 
        layer_surface->surface->mapped != surface->mapped) {
        surface->mapped = layer_surface->surface->mapped;
        arrange_layers(surface->output);
    }
}

void handle_layer_unmap(struct wl_listener *listener, void *data) {
    struct yawc_layer_surface *surface = wl_container_of(listener, surface, unmap);

    if(surface->output && surface->output->server->focused_layer == surface->scene->layer_surface){
         surface->output->server->set_focus_layer(nullptr);
    }
}

void handle_destroy(struct wl_listener *listener, void *data) {
    struct yawc_layer_surface *layer = wl_container_of(listener, layer, destroy);

    scene_descriptor_destroy(&layer->tree->node);

    if (layer->output) {
        arrange_layers(layer->output);
    }

    wl_list_remove(&layer->map.link);
    wl_list_remove(&layer->unmap.link); 
    wl_list_remove(&layer->surface_commit.link);
    wl_list_remove(&layer->destroy.link);
    wl_list_remove(&layer->new_popup.link);

    layer->layer_surface->data = nullptr;

	wl_list_remove(&layer->link);
	delete layer;
}

void handle_layer_new_popup(struct wl_listener *listener, void *data) {
	struct yawc_layer_surface *layer_surface =
		wl_container_of(listener, layer_surface, new_popup);

	struct wlr_xdg_popup *wlr_popup = reinterpret_cast<struct wlr_xdg_popup*>(data);

    create_layer_popup(wlr_popup, layer_surface, layer_surface->tree);
}

void handle_new_layer_shell_surface(struct wl_listener *listener, void *data){
    wlr_log(WLR_DEBUG, "New layer shell surface");

    struct yawc_server *server = wl_container_of(listener, server, new_layer_shell_surface);

    struct wlr_layer_surface_v1 *layer_surface = reinterpret_cast<struct wlr_layer_surface_v1*>(data); 

	auto *wlr_output = layer_surface->output;

    if(!wlr_output){
        wlr_log(WLR_DEBUG, "Panel didn't choose an output, we're gonna grab the first one");

        struct yawc_output *first_output =
            wl_container_of(server->outputs.next, first_output, link);

        wlr_output = first_output->wlr_output;
        layer_surface->output = wlr_output;
    }

    if(!wlr_output || !wlr_output->enabled){
        return;
    }
    
	struct yawc_output *output = static_cast<struct yawc_output*>(wlr_output->data);

	enum zwlr_layer_shell_v1_layer layer_type = layer_surface->pending.layer;

	struct wlr_scene_tree *output_layer = layer_get_scene(server, layer_type);

	struct wlr_scene_layer_surface_v1 *scene_surface =
		wlr_scene_layer_surface_v1_create(output_layer, layer_surface);

	if(!scene_surface){
		wlr_log(WLR_DEBUG, "Couldn't create a new scene surface");
		return;
	}

	struct yawc_layer_surface *xsurface = new yawc_layer_surface{};
	xsurface->scene = scene_surface;
    xsurface->layer_surface = scene_surface->layer_surface;

	if (!scene_descriptor_assign(&scene_surface->tree->node,
			YAWC_SCENE_DESC_LAYER_SHELL, xsurface, 0)) {
		wlr_log(WLR_DEBUG, "Failed to allocate a layer surface descriptor");
		wlr_layer_surface_v1_destroy(layer_surface);
        delete xsurface;
		return;
	}

	xsurface->tree = scene_surface->tree;
	xsurface->scene = scene_surface;
    xsurface->layer_surface->data = xsurface;

	xsurface->output = output;

	wlr_fractional_scale_v1_notify_scale(xsurface->layer_surface->surface,
		output->last_scale);
	wlr_surface_set_preferred_buffer_scale(xsurface->layer_surface->surface,
		ceil(output->last_scale));

	xsurface->surface_commit.notify = handle_layer_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit, &xsurface->surface_commit);
	xsurface->map.notify = handle_layer_map;
	wl_signal_add(&layer_surface->surface->events.map, &xsurface->map);
	xsurface->unmap.notify = handle_layer_unmap;
	wl_signal_add(&layer_surface->surface->events.unmap, &xsurface->unmap);
	xsurface->new_popup.notify = handle_layer_new_popup;
	wl_signal_add(&layer_surface->events.new_popup, &xsurface->new_popup);

	xsurface->destroy.notify = handle_destroy;
	wl_signal_add(&scene_surface->tree->node.events.destroy, &xsurface->destroy);

    wl_list_insert(&output->layer_surfaces, &xsurface->link);

    server->reset_cursor_mode();
}

void on_layer_shell_destroy(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, layer_shell_destroy);

    wl_list_remove(&server->new_layer_shell_surface.link); 
    wl_list_remove(&server->layer_shell_destroy.link); 
}

void yawc_server::create_layer_shell(){
    this->focused_layer = nullptr;
    this->layer_shell = wlr_layer_shell_v1_create(this->wl_display, 4);

    this->new_layer_shell_surface.notify = handle_new_layer_shell_surface;
    wl_signal_add(&this->layer_shell->events.new_surface, &this->new_layer_shell_surface);

    this->layer_shell_destroy.notify = on_layer_shell_destroy;
    wl_signal_add(&this->layer_shell->events.destroy, &this->layer_shell_destroy);
}

