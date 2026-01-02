#include <tuple>

#include "../server.hpp"
#include "../utils.hpp"

#include <assert.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

void on_drag_request(struct wl_listener *listener, void *data)
{
	struct yawc_server *server = wl_container_of(listener, server, drag.events.request);
	struct wlr_seat_request_start_drag_event *event = reinterpret_cast<struct wlr_seat_request_start_drag_event*>(data);

	if (wlr_seat_validate_pointer_grab_serial(server->seat, event->origin, event->serial)) {
		wlr_seat_start_pointer_drag(server->seat, event->drag, event->serial);
	} else {
		wlr_data_source_destroy(event->drag->source);
	}
}

void on_drag_start(struct wl_listener *listener, void *data)
{
	struct yawc_server *server = wl_container_of(listener, server, drag.events.start);

	if(server->drag.running){
		return;
	}
	
	struct wlr_drag *drag = reinterpret_cast<struct wlr_drag*>(data);

	server->drag.running = true;

	if (drag->icon) {
		wlr_scene_drag_icon_create(server->drag.icons, drag->icon);
		wlr_scene_node_raise_to_top(&server->drag.icons->node);
		wlr_scene_node_set_enabled(&server->drag.icons->node, true);
	}

	wl_signal_add(&drag->events.destroy, &server->drag.events.destroy);
}

void on_drag_destroy(struct wl_listener *listener, void *data)
{
	struct yawc_server *server = wl_container_of(listener, server, drag.events.destroy);

	if(!server->drag.running){
		return;
	}

	server->drag.running = false;
	wl_list_remove(&server->drag.events.destroy.link);

	wlr_scene_node_set_enabled(&server->drag.icons->node, false);

	auto *last = utils::previous_toplevel(server);

	utils::focus_toplevel(last);
}

void yawc_server::setup_drag()
{
	this->drag.icons = wlr_scene_tree_create(&this->scene->tree);
	wlr_scene_node_set_enabled(&this->drag.icons->node, false);

	this->drag.events.request.notify = on_drag_request;
	this->drag.events.start.notify = on_drag_start;
	this->drag.events.destroy.notify = on_drag_destroy;

	wl_signal_add(&this->seat->events.request_start_drag, &this->drag.events.request);
	wl_signal_add(&this->seat->events.start_drag, &this->drag.events.start);
}
