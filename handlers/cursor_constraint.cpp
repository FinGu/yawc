#include <tuple>

#include "../utils.hpp"
#include "../window_ops.hpp"
#include "../wm_defs.hpp"

void destroy_cur_constraint(struct yawc_server *sv){
    if(!sv->active_constraint){
        return;
    }

    wlr_pointer_constraint_v1_send_deactivated(sv->active_constraint);

    wl_list_remove(&sv->pointer_constraint_commit.link);
    wl_list_remove(&sv->pointer_constraint_destroy.link);

    sv->active_constraint = nullptr;
}

void check_constraint_region(struct yawc_server *sv){
    struct wlr_pointer_constraint_v1 *constraint = sv->active_constraint;

    if (constraint->type != WLR_POINTER_CONSTRAINT_V1_CONFINED) {
        return;
    }

	pixman_region32_t *region = &constraint->region;

    auto *xdg_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(constraint->surface);

    if(!xdg_toplevel){
        return;
    }

    int gx, gy;
    struct wlr_scene_tree *tree = (struct wlr_scene_tree *)xdg_toplevel->base->data;
    wlr_scene_node_coords(&tree->node, &gx, &gy);

    double local_cursor_x = sv->cursor->x - gx; 
    double local_cursor_y = sv->cursor->y - gy;

    if (pixman_region32_contains_point(region, floor(local_cursor_x), floor(local_cursor_y), NULL)) {
        return;
    }
    
    double target_x = constraint->surface->current.width / 2.0;
    double target_y = constraint->surface->current.height / 2.0;

    double global_target_x = gx + target_x;
    double global_target_y = gy + target_y;
    
    wlr_cursor_warp(sv->cursor, NULL, global_target_x, global_target_y);
}

void handle_pointer_constraint_commit(struct wl_listener *listener, void *data){
	struct yawc_server *sv = wl_container_of(listener, sv, pointer_constraint_commit);

    if (sv->active_constraint) {
        struct wlr_pointer_constraint_v1 *constraint = sv->active_constraint;

        pixman_region32_clear(&constraint->region);
        if(pixman_region32_not_empty(&constraint->current.region)){
            pixman_region32_intersect(&constraint->region, &constraint->surface->input_region, &constraint->current.region);
        } else{
            pixman_region32_copy(&constraint->region, &constraint->surface->input_region);
        }

        check_constraint_region(sv);
    }
}

void handle_pointer_constraint_destroy(struct wl_listener *listener, void *data){
    struct yawc_server* server = wl_container_of(listener, server, pointer_constraint_destroy);
    struct wlr_pointer_constraint_v1 *constraint = (struct wlr_pointer_constraint_v1*)data;

    if (server->active_constraint == constraint) {
        wl_list_remove(&server->pointer_constraint_commit.link);
        wl_list_remove(&server->pointer_constraint_destroy.link);
        server->active_constraint = nullptr;
    }
}

void yawc_server::constrain_cursor(struct wlr_pointer_constraint_v1 *constraint){
    if(this->active_constraint == constraint){
        return;
    }

    destroy_cur_constraint(this);

    if(!constraint){
        return;
    }

    this->active_constraint = constraint;
    
    //sway hack
    if(pixman_region32_not_empty(&constraint->current.region)){
        pixman_region32_intersect(&constraint->region, &constraint->surface->input_region, &constraint->current.region);
    } else{
        pixman_region32_copy(&constraint->region, &constraint->surface->input_region);
    }

	wlr_pointer_constraint_v1_send_activated(constraint);

	this->pointer_constraint_commit.notify = handle_pointer_constraint_commit;
	wl_signal_add(&constraint->surface->events.commit,
		&this->pointer_constraint_commit);

    this->pointer_constraint_destroy.notify = handle_pointer_constraint_destroy;
	wl_signal_add(&constraint->events.destroy,
		&this->pointer_constraint_destroy);

    check_constraint_region(this);
}

void handle_new_pointer_constraint(struct wl_listener *listener, void *data){
    struct yawc_server* server = wl_container_of(listener, server, new_pointer_constraint);

    struct wlr_surface *focus = server->seat->pointer_state.focused_surface;

    if (focus) {
        struct wlr_pointer_constraint_v1 *req = wlr_pointer_constraints_v1_constraint_for_surface(
            server->pointer_constraints, focus, server->seat);
        server->constrain_cursor(req);
    } else {
        server->constrain_cursor(nullptr);
    }
}

void destroy_pointer_constraint(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, pointer_constraint_manager_destroy);

    wl_list_remove(&server->new_pointer_constraint.link); 
    wl_list_remove(&server->pointer_constraint_manager_destroy.link); 
}

void yawc_server::create_pointer_constraint(){
    this->pointer_constraints = wlr_pointer_constraints_v1_create(this->wl_display);

    this->new_pointer_constraint.notify = &handle_new_pointer_constraint;
    wl_signal_add(&this->pointer_constraints->events.new_constraint, &this->new_pointer_constraint);

    this->pointer_constraint_manager_destroy.notify = &destroy_pointer_constraint;
    wl_signal_add(&this->pointer_constraints->events.destroy, &this->pointer_constraint_manager_destroy);
}

void yawc_server::handle_pointer_motion_constraint(double &dx, double &dy){
    if(!this->active_constraint){
        return;
    }

    if (this->active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
        dx = 0;
        dy = 0;
        return;
    }

	struct wlr_surface *surface = NULL;

    auto [node, input] = utils::desktop_node_at(this, this->cursor->x, this->cursor->y);

    if(node->type != WLR_SCENE_NODE_BUFFER){
        return;
    }

	struct wlr_scene_buffer *scene_buffer =
		wlr_scene_buffer_from_node(node);

	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);

    if(!scene_surface){
        return;
    }

	surface = scene_surface->surface;

	if (this->active_constraint->surface != surface) {
		return;
	}
    
    double sx = input.x;
    double sy = input.y;

	double sx_confined, sy_confined;
	if (!wlr_region_confine(&this->active_constraint->region, sx, sy, sx + dx, sy + dy,
			&sx_confined, &sy_confined)) {
		return;
	}

	dx = sx_confined - sx;
	dy = sy_confined - sy;
}
