#include <tuple>

#include "../toplevel.hpp"
#include "../utils.hpp"

void handle_pointer_focus_change(struct wl_listener* listener, void* data){
    struct yawc_server* server = wl_container_of(listener, server, on_pointer_focus_change);

    struct wlr_seat_pointer_focus_change_event* event = reinterpret_cast<struct wlr_seat_pointer_focus_change_event*>(data);

    if (event->new_surface == NULL) {
        wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
    }
}

void handle_seat_request_cursor(struct wl_listener* listener, void* data){
    struct yawc_server* server = wl_container_of(listener, server, on_request_cursor);

    struct wlr_seat_pointer_request_set_cursor_event *event = reinterpret_cast<struct wlr_seat_pointer_request_set_cursor_event*>(data);

	struct wlr_seat_client *focused_client =
		server->seat->pointer_state.focused_client;

	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. */
	if (focused_client == event->seat_client) {
		/* Once we've vetted the client, we can tell the cursor to use the
		 * provided surface as the cursor image. It will set the hardware cursor
		 * on the output that it's currently on and continue to do so as the
		 * cursor moves between outputs. */
		wlr_cursor_set_surface(server->cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
	}
}

void handle_request_set_selection(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, on_request_set_selection);
    struct wlr_seat_request_set_selection_event* event = reinterpret_cast<struct wlr_seat_request_set_selection_event*>(data);

    wlr_seat_set_selection(server->seat, event->source, event->serial);
}

void handle_request_set_primary_selection(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, on_request_set_primary_selection);
    struct wlr_seat_request_set_primary_selection_event* event = reinterpret_cast<struct wlr_seat_request_set_primary_selection_event*>(data);

    wlr_seat_set_primary_selection(server->seat, event->source, event->serial);
}

void handle_seat_destroy(struct wl_listener *listener, void *data){
	struct yawc_server *server = wl_container_of(listener, server, seat_destroy);

	wl_list_remove(&server->on_pointer_focus_change.link);
	wl_list_remove(&server->on_request_cursor.link);
	wl_list_remove(&server->on_request_set_selection.link);
	wl_list_remove(&server->on_request_set_primary_selection.link);

	//wlr_scene_node_destroy(&server->drag.icons->node);
	wl_list_remove(&server->drag.events.request.link);
	wl_list_remove(&server->drag.events.start.link);

	wl_list_remove(&server->seat_destroy.link);

	wlr_seat_destroy(server->seat);
}

void yawc_server::setup_seat()
{
    wlr_log(WLR_DEBUG, "Setting up seat");

    this->seat = wlr_seat_create(this->wl_display, "seat0");

	this->seat_destroy.notify = handle_seat_destroy;
	wl_signal_add(&this->seat->events.destroy, &this->seat_destroy);

    this->on_pointer_focus_change.notify = handle_pointer_focus_change;
    wl_signal_add(&this->seat->pointer_state.events.focus_change, &this->on_pointer_focus_change);

    this->on_request_cursor.notify = handle_seat_request_cursor;
    wl_signal_add(&this->seat->events.request_set_cursor, &this->on_request_cursor);

	this->data_device_manager = wlr_data_device_manager_create(this->wl_display);
    this->primary_selection_manager = wlr_primary_selection_v1_device_manager_create(this->wl_display);

    this->on_request_set_selection.notify = handle_request_set_selection;
    wl_signal_add(&this->seat->events.request_set_selection, &this->on_request_set_selection);

	this->on_request_set_primary_selection.notify = handle_request_set_primary_selection;
    wl_signal_add(&this->seat->events.request_set_primary_selection, &this->on_request_set_primary_selection);

	this->setup_drag();
}

void yawc_server::set_focus_surface(struct wlr_surface *surface) {
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(this->seat);

	if (surface && keyboard) {
		wlr_seat_keyboard_notify_enter(this->seat, surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
	} else {
		wlr_seat_keyboard_notify_clear_focus(this->seat);
	}
}

void yawc_server::set_focus_layer(struct wlr_layer_surface_v1 *layer) {
	struct wlr_surface *prev_surface = this->seat->keyboard_state.focused_surface;
    if (prev_surface) {
        struct wlr_xdg_toplevel* prev_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
        if (prev_toplevel) {
            wlr_xdg_toplevel_set_activated(prev_toplevel, false);
        }
    }

	if (!layer && this->focused_layer) {
		this->focused_layer = nullptr;
        wlr_seat_keyboard_notify_clear_focus(this->seat);
		if (!wl_list_empty(&this->toplevels)) {
            struct yawc_toplevel *previous = wl_container_of(this->toplevels.next, previous, link);
            if (previous) {
                utils::focus_toplevel(previous);
            }
        }

		return;
	} else if (!layer) {
		return;
	}

	if (layer->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP &&
			layer->current.keyboard_interactive
			== ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {
		this->has_exclusive_layer = true;
	}

	if (this->focused_layer == layer) {
		return;
	}

	this->set_focus_surface(layer->surface);
	this->focused_layer = layer;
}

void on_pointer_destroy(struct wl_listener *listener, void *data){
    struct yawc_pointer *pointer = wl_container_of(listener, pointer, link);

    wl_list_remove(&pointer->destroy.link);
    wl_list_remove(&pointer->link);
    delete pointer;
}

void yawc_server::handle_new_input(struct wl_listener* listener, void* data){
    wlr_log(WLR_DEBUG, "Handling new input");

    struct wlr_input_device* input = reinterpret_cast<struct wlr_input_device*>(data);

    if (input->type == WLR_INPUT_DEVICE_KEYBOARD) {
		yawc_keyboard *keyboard = this->handle_keyboard(input);
		this->load_keyboard_cfg(keyboard);
    } else if (input->type == WLR_INPUT_DEVICE_POINTER) {
		yawc_pointer *pointer =  this->handle_pointer(input);
		this->load_pointer_cfg(pointer);
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;

    if (!wl_list_empty(&this->keyboards)) {
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
        wlr_log(WLR_DEBUG, "Setting keyboard capability");
    }

    wlr_seat_set_capabilities(this->seat, caps);
}
