#include <tuple>

#include "../utils.hpp"
#include "../window_ops.hpp"
#include "../wm_defs.hpp"

void warp_to_constraint_cursor_hint(struct yawc_server *server) {
	struct wlr_pointer_constraint_v1 *constraint = server->active_constraint;

	if(!constraint->current.cursor_hint.enabled){
		return;
	}

	double sx = constraint->current.cursor_hint.x;
	double sy = constraint->current.cursor_hint.y;

	struct yawc_toplevel *toplevel = utils::get_toplevel_from_wlr_surface(constraint->surface);
	if (!toplevel) {
		return;
	}

	int gx, gy;
	struct wlr_scene_tree *tree = toplevel->scene_tree;
   	wlr_scene_node_coords(&tree->node, &gx, &gy);

	double lx = sx + gx;
	double ly = sy + gy;

	wlr_cursor_warp(server->cursor, NULL, lx, ly);

	// Warp the pointer as well, so that on the next pointer rebase we don't
	// send an unexpected synthetic motion event to clients.
	wlr_seat_pointer_warp(constraint->seat, sx, sy);
}

void handle_pointer_constraint_commit(struct wl_listener *listener, void *data){
	struct yawc_server *sv = wl_container_of(listener, sv, pointer_constraint_commit_list);
	(void)data;
}

void handle_pointer_constraint_destroy(struct wl_listener *listener, void *data){
    struct yawc_pointer_constraint* constraint = wl_container_of(listener, constraint, destroy);

	struct wlr_pointer_constraint_v1 *wlr_constraint = constraint->wlr_constraint;
	struct yawc_server *server = constraint->server;

	wl_list_remove(&constraint->destroy.link);

	if (server->active_constraint == wlr_constraint) {
		warp_to_constraint_cursor_hint(server);

		if (server->pointer_constraint_commit_list.link.next != NULL) {
			wl_list_remove(&server->pointer_constraint_commit_list.link);
		}

		wl_list_init(&server->pointer_constraint_commit_list.link);

		server->active_constraint = nullptr;
	}

	delete constraint;
}

void yawc_server::constrain_cursor(struct wlr_pointer_constraint_v1 *constraint){
    if(this->active_constraint == constraint){
        return;
    }

	wl_list_remove(&this->pointer_constraint_commit_list.link);
	if (this->active_constraint) {
		if(constraint == nullptr){
			warp_to_constraint_cursor_hint(this);
		}

		wlr_pointer_constraint_v1_send_deactivated(
			this->active_constraint);
	}

	this->active_constraint = constraint;

	if (constraint == NULL) {
		wl_list_init(&this->pointer_constraint_commit_list.link);
		return;
	}

	wlr_pointer_constraint_v1_send_activated(constraint);

	this->pointer_constraint_commit_list.notify = handle_pointer_constraint_commit;
	wl_signal_add(&constraint->surface->events.commit,
		&this->pointer_constraint_commit_list);
}

void handle_new_pointer_constraint(struct wl_listener *listener, void *data){
    struct yawc_server* server = wl_container_of(listener, server, new_pointer_constraint);
	struct wlr_pointer_constraint_v1 *tmp_constraint = reinterpret_cast<struct wlr_pointer_constraint_v1*>(data);

    struct wlr_surface *focus = server->seat->pointer_state.focused_surface;

	struct yawc_pointer_constraint *constraint = new yawc_pointer_constraint{};
	constraint->wlr_constraint = tmp_constraint;
	constraint->server = server;

	constraint->destroy.notify = handle_pointer_constraint_destroy;
	wl_signal_add(&tmp_constraint->events.destroy, &constraint->destroy);
	
    if (focus && focus == tmp_constraint->surface){
        server->constrain_cursor(tmp_constraint);
    } 
}

void destroy_pointer_constraint_manager(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, pointer_constraint_manager_destroy);

    wl_list_remove(&server->new_pointer_constraint.link); 
    wl_list_remove(&server->pointer_constraint_manager_destroy.link); 
}

void yawc_server::create_pointer_constraint(){
	this->active_constraint = nullptr;
	wl_list_init(&this->pointer_constraint_commit_list.link);

    this->pointer_constraints = wlr_pointer_constraints_v1_create(this->wl_display);

    this->new_pointer_constraint.notify = &handle_new_pointer_constraint;
    wl_signal_add(&this->pointer_constraints->events.new_constraint, &this->new_pointer_constraint);

    this->pointer_constraint_manager_destroy.notify = &destroy_pointer_constraint_manager;
    wl_signal_add(&this->pointer_constraints->events.destroy, &this->pointer_constraint_manager_destroy);
}

//true for success
bool yawc_server::handle_pointer_motion_constraint(double &dx, double &dy){
    if(!this->active_constraint){
        return true;
    }

	//we still need to send the event to the surface anyways
    if (this->active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
        dx = 0;
        dy = 0;
		return true;
	}

	struct wlr_surface *surface = NULL;

    auto [node, input] = utils::desktop_node_at(this, this->cursor->x, this->cursor->y);

    if(!node || node->type != WLR_SCENE_NODE_BUFFER){
        return false;
    }

	struct wlr_scene_buffer *scene_buffer =
		wlr_scene_buffer_from_node(node);

	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);

    if(!scene_surface){
        return false;
    }

	surface = scene_surface->surface;

	if (this->active_constraint->surface != surface) {
		return false;
	}
    
    double sx = input.x;
    double sy = input.y;

	double sx_confined, sy_confined;
	if (!wlr_region_confine(&this->active_constraint->region, sx, sy, sx + dx, sy + dy,
			&sx_confined, &sy_confined)) {
		return false;
	}

	dx = sx_confined - sx;
	dy = sy_confined - sy;

	return true;
}
