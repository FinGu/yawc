#include <linux/input-event-codes.h>
#include <unistd.h>

#include "../utils.hpp"
#include "../window_ops.hpp"
#include "../wm_defs.hpp"

void popup_unconstrain(struct yawc_popup *popup) {
	struct yawc_toplevel *toplevel = popup->toplevel;
	struct wlr_xdg_popup *wlr_popup = popup->xdg_popup;

    auto *toplevel_geometry = &toplevel->xdg_toplevel->base->geometry;

    auto *popup_geometry = &popup->xdg_popup->base->geometry;

	struct wlr_output *wlr_output = wlr_output_layout_output_at(
			toplevel->server->output_layout,
			toplevel_geometry->x + popup_geometry->x,
			toplevel_geometry->y + popup_geometry->y);

    if(!wlr_output){
        return;
    }

    auto *output = reinterpret_cast<struct yawc_output*>(wlr_output->data);
    
    struct wlr_box output_box;
    wlr_output_layout_get_box(toplevel->server->output_layout, output->wlr_output, &output_box);

	struct wlr_box output_toplevel_sx_box = {
		.x = output_box.x - toplevel_geometry->x + popup_geometry->x,
		.y = output_box.y - toplevel_geometry->y + popup_geometry->y,
		.width = output_box.width,
		.height = output_box.height,
	};

	wlr_xdg_popup_unconstrain_from_box(wlr_popup, &output_toplevel_sx_box);
}

void handle_toplevel_map(struct wl_listener* listener, void* data) {
    wlr_log(WLR_DEBUG, "Mapping toplevel");

    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, events.map);

    toplevel->map();
}

void handle_toplevel_unmap(struct wl_listener* listener, void* data){
    wlr_log(WLR_DEBUG, "Unmapping toplevel");

    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, events.unmap);

    toplevel->unmap();
}

void handle_toplevel_commit(struct wl_listener* listener, void* data){
    wlr_log(WLR_DEBUG, "Toplevel commit");
    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, events.commit);
    
    if (toplevel->xdg_toplevel->base->initial_commit) {
		wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
        return;
    }

    toplevel->commit();
}

void handle_toplevel_move(struct wl_listener* listener, void* data) { 
    wlr_log(WLR_DEBUG, "Move requested");

    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, events.request_move);

    if(!toplevel->xdg_toplevel->base->initialized){
        return;
    }

    toplevel->request_move();
}

void handle_toplevel_resize(struct wl_listener* listener, void* data)
{
    wlr_log(WLR_DEBUG, "Resize requested");

    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, events.request_resize);

    if(!toplevel->xdg_toplevel->base->initialized){
        return;
    }

    struct wlr_xdg_toplevel_resize_event* event = reinterpret_cast<struct wlr_xdg_toplevel_resize_event*>(data);

    toplevel->request_resize(event->edges);
}

void handle_toplevel_maximize(struct wl_listener* listener, void* data)
{
    wlr_log(WLR_DEBUG, "Maximize requested");

    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, events.request_maximize);

    if(!toplevel->xdg_toplevel->base->initialized){
        return;
    }

    toplevel->request_maximize(toplevel->xdg_toplevel->requested.maximized);
}

void handle_toplevel_minimize(struct wl_listener* listener, void* data){
    wlr_log(WLR_DEBUG, "Minimize requested");

    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, events.request_minimize);

    if(!toplevel->xdg_toplevel->base->initialized){
        return;
    }

    toplevel->request_minimize(toplevel->xdg_toplevel->requested.minimized);
}

void handle_toplevel_fullscreen(struct wl_listener *listener, void *data){
    wlr_log(WLR_DEBUG, "Fullscreen requested");

    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, events.request_fullscreen);

    if(!toplevel->xdg_toplevel->base->initialized){
        return;
    }

    toplevel->request_fullscreen(toplevel->xdg_toplevel->requested.fullscreen);
}

void handle_toplevel_destroy(struct wl_listener* listener, void* data)
{
    wlr_log(WLR_DEBUG, "Destroying toplevel");
    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, events.destroy);

    if (toplevel == toplevel->server->grabbed_toplevel) {
        toplevel->server->reset_cursor_mode();
    }

    if(toplevel->has_resize_grips){
        for(size_t i = 0; i < 8; ++i){
            scene_descriptor_destroy(toplevel->resize_grips[i]);

            toplevel->resize_grips[i] = nullptr;
        }
    }

    scene_descriptor_destroy(&toplevel->scene_tree->node);
    wlr_scene_node_destroy(&toplevel->scene_tree->node);
    
    wl_list_remove(&toplevel->events.map.link);
    wl_list_remove(&toplevel->events.unmap.link);
    wl_list_remove(&toplevel->events.commit.link);
    wl_list_remove(&toplevel->events.request_move.link);
    wl_list_remove(&toplevel->events.request_resize.link);
    wl_list_remove(&toplevel->events.request_maximize.link);
    wl_list_remove(&toplevel->events.request_minimize.link);
    wl_list_remove(&toplevel->events.request_fullscreen.link);
    wl_list_remove(&toplevel->events.new_popup.link);

    wl_list_remove(&toplevel->events.destroy.link);

    toplevel->scene_tree = nullptr;

    delete toplevel;
}

void handle_popup_commit(struct wl_listener* listener, void* data)
{
    struct yawc_popup* popup = wl_container_of(listener, popup, commit);

    if (popup->xdg_popup->base->initial_commit) {
        popup_unconstrain(popup);

        wl_list_remove(&popup->commit.link);
		popup->commit.notify = NULL;
    }
}

void handle_popup_destroy(struct wl_listener* listener, void* data)
{
    struct yawc_popup* popup = wl_container_of(listener, popup, destroy);
    wl_list_remove(&popup->new_popup.link);
	wl_list_remove(&popup->destroy.link);

    if(popup->commit.notify){
	    wl_list_remove(&popup->commit.link);
    }

	wl_list_remove(&popup->reposition.link);

    //scene_descriptor_destroy(&popup->scene_tree->node);
    
    delete popup;
}

void handle_popup_reposition(struct wl_listener* listener, void* data){
    struct yawc_popup *popup = wl_container_of(listener, popup, reposition);
	popup_unconstrain(popup);
}

void handle_new_toplevel(struct wl_listener* listener, void* data){
    struct yawc_server* server = wl_container_of(listener, server, new_xdg_toplevel_listener);
    server->new_xdg_toplevel(listener, data);
}

struct yawc_popup *create_xdg_popup(struct wlr_xdg_popup *wlr_popup, struct yawc_toplevel *toplevel, struct wlr_scene_tree *parent);

void xpopup_handle_new_popup(struct wl_listener *listener, void *data) {
    struct yawc_popup *parent_popup = wl_container_of(listener, parent_popup, new_popup);
    struct wlr_xdg_popup *wlr_popup = reinterpret_cast<struct wlr_xdg_popup*>(data);

    create_xdg_popup(wlr_popup, parent_popup->toplevel, parent_popup->scene_tree);
}

struct yawc_popup *create_xdg_popup(struct wlr_xdg_popup *wlr_popup, struct yawc_toplevel *toplevel, struct wlr_scene_tree *parent) {
	struct wlr_xdg_surface *xdg_surface = wlr_popup->base;

	struct yawc_popup *popup = new yawc_popup{};

	if (!popup) {
		return nullptr;
	}
    
    popup->xdg_popup = wlr_popup;
    popup->toplevel = toplevel;

	popup->scene_tree = wlr_scene_xdg_surface_create(parent, wlr_popup->base);

	if (!popup->scene_tree) {
		delete popup;
		return nullptr;
	}

    xdg_surface->data = popup->scene_tree;

    popup->commit.notify = handle_popup_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &popup->commit);

    popup->new_popup.notify = xpopup_handle_new_popup;
	wl_signal_add(&xdg_surface->events.new_popup, &popup->new_popup);

    popup->reposition.notify = handle_popup_reposition;
	wl_signal_add(&wlr_popup->events.reposition, &popup->reposition);

    popup->destroy.notify = handle_popup_destroy;
	wl_signal_add(&wlr_popup->events.destroy, &popup->destroy);

	return popup;
}

void handle_toplevel_new_popup(struct wl_listener* listener, void* data){
    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, events.new_popup);
    auto* xdg_popup = reinterpret_cast<struct wlr_xdg_popup*>(data);

    create_xdg_popup(xdg_popup, toplevel, toplevel->scene_tree);
}

void handle_toplevel_listener_destroy(struct wl_listener* listener, void* data)
{
    struct yawc_server* server = wl_container_of(listener, server, xdg_toplevel_listener_destroy);

    wl_list_remove(&server->new_xdg_toplevel_listener.link);
    wl_list_remove(&server->xdg_toplevel_listener_destroy.link);
}

void yawc_server::create_xdg_shell()
{
    wlr_log(WLR_DEBUG, "Initializing toplevels");

    this->xdg_shell = wlr_xdg_shell_create(this->wl_display, 3);

    this->new_xdg_toplevel_listener.notify = handle_new_toplevel;
    wl_signal_add(&this->xdg_shell->events.new_toplevel,
        &this->new_xdg_toplevel_listener);

    this->xdg_toplevel_listener_destroy.notify = handle_toplevel_listener_destroy;
    wl_signal_add(&this->xdg_shell->events.destroy, &this->xdg_toplevel_listener_destroy);

    this->foreign_toplevel_manager = wlr_foreign_toplevel_manager_v1_create(this->wl_display);
}

void yawc_server::new_xdg_toplevel(struct wl_listener* listener, void* data)
{
    wlr_log(WLR_DEBUG, "New xdg toplevel added");

    struct wlr_xdg_toplevel* xdg_toplevel = reinterpret_cast<struct wlr_xdg_toplevel*>(data);

    yawc_toplevel *toplevel = create_toplevel_xdg(this, xdg_toplevel);

    if(!toplevel){
        return;
    }

    toplevel->events.map.notify = handle_toplevel_map;
    toplevel->events.unmap.notify = handle_toplevel_unmap;
    toplevel->events.commit.notify = handle_toplevel_commit;
    toplevel->events.request_move.notify = handle_toplevel_move;
    toplevel->events.request_resize.notify = handle_toplevel_resize;
    toplevel->events.request_maximize.notify = handle_toplevel_maximize;
    toplevel->events.request_minimize.notify = handle_toplevel_minimize;
    toplevel->events.request_fullscreen.notify = handle_toplevel_fullscreen;
    toplevel->events.new_popup.notify = handle_toplevel_new_popup;
    toplevel->events.destroy.notify = handle_toplevel_destroy;

    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->events.map);
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->events.unmap);
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->events.commit);

    wl_signal_add(&xdg_toplevel->base->events.new_popup, &toplevel->events.new_popup);

    wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->events.request_move);
    wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->events.request_resize);
    wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel->events.request_maximize);
    wl_signal_add(&xdg_toplevel->events.request_minimize, &toplevel->events.request_minimize);
    wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->events.request_fullscreen);
    wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->events.destroy);
}
