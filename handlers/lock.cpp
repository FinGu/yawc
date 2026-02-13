#include <tuple>

#include "../lock.hpp"

#include "../toplevel.hpp"
#include "../utils.hpp"
#include "../layer.hpp"

struct yawc_session_lock_output{
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *background;

	struct yawc_output *output;

	struct yawc_server *server;

	struct wl_list link;

	struct wl_listener destroy;

	struct wlr_session_lock_surface_v1 *surface;

	struct wl_listener surface_destroy;
	struct wl_listener surface_map;
};

void focus_surface(struct yawc_server *server, wlr_surface *surface){
	server->set_focus_surface(surface);
	server->cur_lock.focused = surface;
}

void lock_output_reconfigure(struct yawc_session_lock_output *output) {
	int width = output->output->last_width;
	int height = output->output->last_height;

	wlr_scene_rect_set_size(output->background, width, height);

	if (output->surface) {
		wlr_session_lock_surface_v1_configure(output->surface, width, height);
	}
}

void refocus_output(struct yawc_session_lock_output *output) {
	auto *server = output->server;

	if (server->cur_lock.focused == output->surface->surface) {
		struct wlr_surface *next_focus = NULL;

		struct yawc_session_lock_output *candidate;
		wl_list_for_each(candidate, &server->cur_lock.outputs, link) {
			if (candidate == output || !candidate->surface) {
				continue;
			}

			if (candidate->surface->surface->mapped) {
				next_focus = candidate->surface->surface;
				break;
			}
		}

		focus_surface(server, next_focus);
	}
}

void session_lock_destroy(yawc_server *sv) {
	auto *lock = &sv->cur_lock;

	struct yawc_session_lock_output *lock_output, *tmp_lock_output;
	wl_list_for_each_safe(lock_output, tmp_lock_output, &lock->outputs, link) {
		// destroying the node will also destroy the whole lock output
		wlr_scene_node_destroy(&lock_output->tree->node);
	}

	sv->cur_lock.lock = NULL;
	
	if (!lock->crashed) {
		wl_list_remove(&lock->destroy.link);
		wl_list_remove(&lock->unlock.link);
		wl_list_remove(&lock->new_surface.link);
	}
}

yawc_session_lock_output *die(){
	wlr_log(WLR_ERROR, "Failed to lock");
	exit(-1);
	return nullptr;
}

void session_lock_output_destroy(struct yawc_session_lock_output *output) {
	if (output->surface) {
		refocus_output(output);
		wl_list_remove(&output->surface_destroy.link);
		wl_list_remove(&output->surface_map.link);
	}

	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);

	delete output;
}

void lock_node_handle_destroy(struct wl_listener *listener, void *data) {
	struct yawc_session_lock_output *output =
		wl_container_of(listener, output, destroy);
	session_lock_output_destroy(output);
}

yawc_session_lock_output *session_lock_output_create(yawc_server *sv, yawc_output *output){
	auto *lock_output = new yawc_session_lock_output{};

	if(!lock_output){
		return die();
	}

	struct wlr_scene_tree *tree = wlr_scene_tree_create(sv->layers.screenlock);

	if (!tree) {
		delete lock_output; 
		return die();
	}

	struct wlr_scene_rect *background = wlr_scene_rect_create(tree, 0, 0, (float[4]){
		sv->cur_lock.crashed ? 1.f : 0.f,
		0.f,
		0.f,
		1.f,
	});

	if (!background) {
		wlr_scene_node_destroy(&tree->node);
		delete lock_output;
		return die();
	}

	lock_output->server = sv;

	lock_output->output = output;
	lock_output->tree = tree;
	lock_output->background = background;

	lock_output->destroy.notify = lock_node_handle_destroy;
	wl_signal_add(&tree->node.events.destroy, &lock_output->destroy);

	lock_output_reconfigure(lock_output);

	wl_list_insert(&sv->cur_lock.outputs, &lock_output->link);

	return lock_output;
}

void handle_surface_map(struct wl_listener *listener, void *data) {
	struct yawc_session_lock_output *surf = wl_container_of(listener, surf, surface_map);
	auto *server = surf->server;

	if (server->cur_lock.focused == NULL) {
		focus_surface(server, surf->surface->surface);
	}

	auto [node, input] = utils::desktop_node_at(server, server->cursor->x, server->cursor->y);

	wlr_seat_pointer_notify_enter(server->seat, surf->surface->surface, input.x, input.y);
}

void handle_lock_surface_destroy(struct wl_listener *listener, void *data) {
	struct yawc_session_lock_output *output =
		wl_container_of(listener, output, surface_destroy);
	refocus_output(output);

	if(!output->surface){
		return;
	}

	output->surface = NULL;
	wl_list_remove(&output->surface_destroy.link);
	wl_list_remove(&output->surface_map.link);
}

void handle_new_lock_surface(struct wl_listener *listener, void *data){
	struct yawc_session_lock *session_lock = wl_container_of(listener, session_lock, new_surface);

	struct wlr_session_lock_surface_v1 *lock_surface = reinterpret_cast<struct wlr_session_lock_surface_v1*>(data);

	struct yawc_output *output = reinterpret_cast<struct yawc_output*>(lock_surface->output->data);

	struct yawc_session_lock_output *current_lock_output, *lock_output = NULL;
	wl_list_for_each(current_lock_output, &session_lock->outputs, link) {
		if (current_lock_output->output == output) {
			lock_output = current_lock_output;
			break;
		}
	}

	lock_output->surface = lock_surface;

	wlr_scene_subsurface_tree_create(lock_output->tree, lock_surface->surface);

	lock_output->surface_destroy.notify = handle_lock_surface_destroy;
	wl_signal_add(&lock_surface->events.destroy, &lock_output->surface_destroy);
	lock_output->surface_map.notify = handle_surface_map;
	wl_signal_add(&lock_surface->surface->events.map, &lock_output->surface_map);

	lock_output_reconfigure(lock_output);
}

void handle_unlock(struct wl_listener *listener, void *data) {
	struct yawc_session_lock *lock = wl_container_of(listener, lock, unlock);

	auto *server = reinterpret_cast<struct yawc_server*>(lock->lock->data);

	session_lock_destroy(server);
	
	if(!wl_list_empty(&server->toplevels)){
		struct yawc_toplevel *toplevel;

		auto *out_toplevel = wl_container_of(server->toplevels.prev, toplevel, link);

		utils::focus_toplevel(out_toplevel);
	}

	struct yawc_output *output;
	wl_list_for_each(output, &server->outputs, link){
		arrange_layers(output);
	}
	
	server->update_idle_inhibitors();
}

static void handle_abandon(struct wl_listener *listener, void *data) {
	struct yawc_session_lock *lock = wl_container_of(listener, lock, destroy);

	struct yawc_session_lock_output *lock_output;
	wl_list_for_each(lock_output, &lock->outputs, link) {
		wlr_scene_rect_set_color(lock_output->background,
			(float[4]){ 1.f, 0.f, 0.f, 1.f });
	}

	lock->crashed = true;
	wl_list_remove(&lock->destroy.link);
	wl_list_remove(&lock->unlock.link);
	wl_list_remove(&lock->new_surface.link);
}

void handle_new_lock(struct wl_listener *listener, void *data){
	struct yawc_server *server = wl_container_of(listener, server, new_session_lock);

	struct wlr_session_lock_v1 *lock = reinterpret_cast<struct wlr_session_lock_v1*>(data);
	//struct wl_client *client = wl_resource_get_client(lock->resource);

	if(server->cur_lock.lock){
		if(!server->cur_lock.crashed){
			wlr_session_lock_v1_destroy(lock);
			return;
		}

		session_lock_destroy(server);
	}

	server->set_focus_layer(nullptr);
    server->set_focus_surface(nullptr);
	server->constrain_cursor(nullptr);

	wl_list_init(&server->cur_lock.outputs);

	struct yawc_output *output;
	wl_list_for_each(output, &server->outputs, link) {
        session_lock_output_create(server, output);
	}
	
	server->cur_lock.new_surface.notify = handle_new_lock_surface;
	wl_signal_add(&lock->events.new_surface, &server->cur_lock.new_surface);

	server->cur_lock.unlock.notify = handle_unlock;
	wl_signal_add(&lock->events.unlock, &server->cur_lock.unlock);

	server->cur_lock.destroy.notify = handle_abandon;
	wl_signal_add(&lock->events.destroy, &server->cur_lock.destroy);

	lock->data = server;

	wlr_session_lock_v1_send_locked(lock);
	server->cur_lock.lock = lock;
	server->cur_lock.crashed = false;
	server->cur_lock.focused = nullptr;

	// The lock screen covers everything, so check if any active inhibition got
	// deactivated due to lost visibility.
	// sway_idle_inhibit_v1_check_active();
	
	/* We're ignoring this for now, might check if the one requesting inhibition is the screenlock itself*/
	server->update_idle_inhibitors();
}

void handle_lock_manager_destroy(struct wl_listener *listener, void *data){
	struct yawc_server *server = wl_container_of(listener, server, session_lock_manager_destroy);	

	if (server->cur_lock.lock) {
		session_lock_destroy(server);
	}

	wl_list_remove(&server->new_session_lock.link);
	wl_list_remove(&server->session_lock_manager_destroy.link);
}

void arrange_locks(struct yawc_server *sv){
	if (sv->cur_lock.lock == nullptr) {
		return;
	}

	struct yawc_session_lock_output *lock_output;
	wl_list_for_each(lock_output, &sv->cur_lock.outputs, link) {
		lock_output_reconfigure(lock_output);
	}
}

void yawc_server::create_session_lock_manager(){
	this->cur_lock = {0};

	this->session_lock_manager = wlr_session_lock_manager_v1_create(this->wl_display);

	this->new_session_lock.notify = handle_new_lock; 
	wl_signal_add(&this->session_lock_manager->events.new_lock, &this->new_session_lock);

	this->session_lock_manager_destroy.notify = handle_lock_manager_destroy;
	wl_signal_add(&this->session_lock_manager->events.destroy, &this->session_lock_manager_destroy);
}

