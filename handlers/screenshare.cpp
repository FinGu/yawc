#include "toplevel.hpp"

void handle_new_foreign_toplevel_capture_request(struct wl_listener *listener, void *data) {
	struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request = 
        reinterpret_cast<struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request*>(data);

	struct yawc_toplevel *toplevel = reinterpret_cast<struct yawc_toplevel*>(request->toplevel_handle->data);

	auto *server = toplevel->server;

	if (toplevel->image_capture_source == nullptr) {
		toplevel->image_capture_source = wlr_ext_image_capture_source_v1_create_with_scene_node(
			&toplevel->image_capture_scene->tree.node, server->wl_event_loop, server->allocator, server->renderer);

		if (toplevel->image_capture_source == nullptr) {
			return;
		}
	}

	wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(request, toplevel->image_capture_source);
}

void handle_foreign_toplevel_capture_manager_destroy(struct wl_listener *listener, void *data){
	struct yawc_server *server = wl_container_of(listener, server, foreign_toplevel_capture_handler_destroy);

	wl_list_remove(&server->new_foreign_toplevel_capture_request.link);
	wl_list_remove(&server->foreign_toplevel_capture_handler_destroy.link);
}

void yawc_server::setup_screensharing(){
    wlr_screencopy_manager_v1_create(this->wl_display); //old protocol

    wlr_ext_image_copy_capture_manager_v1_create(this->wl_display, 1);
	wlr_ext_output_image_capture_source_manager_v1_create(this->wl_display, 1);

    this->ext_foreign_toplevel_image_capture_source_manager_v1 =
	    wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(this->wl_display, 1);

	this->new_foreign_toplevel_capture_request.notify = handle_new_foreign_toplevel_capture_request;
	wl_signal_add(&this->ext_foreign_toplevel_image_capture_source_manager_v1->events.new_request, &this->new_foreign_toplevel_capture_request);

	this->foreign_toplevel_capture_handler_destroy.notify = handle_foreign_toplevel_capture_manager_destroy;
	wl_signal_add(&this->ext_foreign_toplevel_image_capture_source_manager_v1->events.destroy, &this->foreign_toplevel_capture_handler_destroy);
}

