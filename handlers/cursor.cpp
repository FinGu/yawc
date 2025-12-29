#include <libinput.h>
#include <tuple>

#include "../utils.hpp"
#include "../window_ops.hpp"
#include "../wm_defs.hpp"

bool yawc_server::do_mouse_operation()
{
    if (!utils::toplevel_not_empty(this->grabbed_toplevel)) {
        return false;
    }

    if (!this->grabbed_toplevel->scene_tree) {
        return false;
    }

    if (this->current_mouse_operation == MOVING) {
        int new_x = this->cursor->x - this->grabbed_mov_x;
        int new_y = this->cursor->y - this->grabbed_mov_y;

        wlr_scene_node_set_position(&this->grabbed_toplevel->scene_tree->node,
            new_x,
            new_y);

        struct wlr_box geo_box = this->grabbed_toplevel->xdg_toplevel->base->geometry;
        
        //geo_box.x = new_x;
        //geo_box.y = new_y;

        //wlr_xdg_toplevel_set_size(this->grabbed_toplevel->xdg_toplevel, geo_box.width, geo_box.height);
        
        return true;
    }

    if (this->current_mouse_operation == RESIZING) {
        wlr_log(WLR_DEBUG, "Resizing window");

        double border_x = this->cursor->x - this->grabbed_res_x;
        double border_y = this->cursor->y - this->grabbed_res_y;
        int new_left = this->grabbed_geo_box.x;
        int new_right = this->grabbed_geo_box.x + this->grabbed_geo_box.width;
        int new_top = this->grabbed_geo_box.y;
        int new_bottom = this->grabbed_geo_box.y + this->grabbed_geo_box.height;

        if (this->resize_edges & WLR_EDGE_TOP) {
            new_top = border_y;
            if (new_top >= new_bottom) {
                new_top = new_bottom - 1;
            }
        } else if (this->resize_edges & WLR_EDGE_BOTTOM) {
            new_bottom = border_y;
            if (new_bottom <= new_top) {
                new_bottom = new_top + 1;
            }
        }
        if (this->resize_edges & WLR_EDGE_LEFT) {
            new_left = border_x;
            if (new_left >= new_right) {
                new_left = new_right - 1;
            }
        } else if (this->resize_edges & WLR_EDGE_RIGHT) {
            new_right = border_x;
            if (new_right <= new_left) {
                new_right = new_left + 1;
            }
        }

        struct wlr_box geo_box = this->grabbed_toplevel->xdg_toplevel->base->geometry;

        wlr_scene_node_set_position(&this->grabbed_toplevel->scene_tree->node,
            new_left - geo_box.x, new_top - geo_box.y);

        int new_width = new_right - new_left;
        int new_height = new_bottom - new_top;

        geo_box.width = new_width;
        geo_box.height = new_height;

        geo_box.x = new_left;
        geo_box.y = new_top;

        wlr_xdg_toplevel_set_size(this->grabbed_toplevel->xdg_toplevel, geo_box.width, geo_box.height);

        return true;
    }

    return false;
}

void handle_cursor_button(struct wl_listener* listener,
    void* data)
{
    struct yawc_server* server = wl_container_of(listener, server, cursor_button_listener);
    struct wlr_pointer_button_event* event = reinterpret_cast<struct wlr_pointer_button_event*>(data);

    wlr_log(WLR_DEBUG, "Handling cursor button");

    utils::wake_up_from_idle(server);

    auto [_, input_on_surface] = utils::desktop_toplevel_at(server, server->cursor->x, server->cursor->y);

    wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);

    wlr_seat_pointer_notify_frame(server->seat);

    wlr_layer_surface_v1 *lsurface = 
        utils::toplevel_layer_surface_from_surface(input_on_surface.surface);

    if(lsurface){
        //if(lsurface->current.keyboard_interactive){
            server->set_focus_layer(lsurface);
        //}
        return;
    }
    
    if(event->state == WL_POINTER_BUTTON_STATE_RELEASED){
        server->reset_cursor_mode();
    }
    
    if(server->wm.handle && server->wm.callbacks.on_pointer_button){
        wm_pointer_event_t out_event = wm_create_pointer_event(server, event->button, utils::pointer_pressed(event)); 

        server->wm.callbacks.on_pointer_button(&out_event);

        wm_destroy_pointer_event(&out_event);
    }
}

void handle_cursor_axis(struct wl_listener* listener, void* data){
    struct yawc_server *server = wl_container_of(listener, server, on_axis_cursor_motion);
    struct wlr_pointer_axis_event *event = reinterpret_cast<struct wlr_pointer_axis_event*>(data);
    wlr_seat_pointer_notify_axis(server->seat, event->time_msec, event->orientation, event->delta, event->delta_discrete, event->source, event->relative_direction);
    utils::wake_up_from_idle(server);
}

void yawc_server::handle_pointer_motion(struct wl_listener* listener, void* data,
    bool absolute)
{
    wlr_pointer* pointer = nullptr;
    double dx, dy, 
           unacc_dx, unacc_dy; //unaccelerated
    uint32_t time;

    if (absolute) {
        auto* motion = reinterpret_cast<struct wlr_pointer_motion_absolute_event*>(data);
        pointer = motion->pointer;
        time = motion->time_msec;

    	double lx, ly;
	    wlr_cursor_absolute_to_layout_coords(this->cursor, &pointer->base,
			motion->x, motion->y, &lx, &ly);

        dx = lx - this->cursor->x;
        dy = ly - this->cursor->y;
        unacc_dx = dx;
        unacc_dy = dy;
    } else {
        auto* motion = reinterpret_cast<struct wlr_pointer_motion_event*>(data);
        pointer = motion->pointer;
        time = motion->time_msec;

        dx = motion->delta_x;
        dy = motion->delta_y;
        unacc_dx = motion->unaccel_dx;
        unacc_dy = motion->unaccel_dy;
    }

	wlr_relative_pointer_manager_v1_send_relative_motion(
		this->relative_pointer_manager,
		this->seat, (uint64_t)time * 1000,
		dx, dy, unacc_dx, unacc_dy);

    handle_pointer_motion_constraint(dx, dy);

    wlr_cursor_move(this->cursor, &pointer->base, dx, dy);

    utils::wake_up_from_idle(this);

    if (do_mouse_operation()) {
        return;
    }

    if(this->drag.icons){
        wlr_scene_node_set_position(&this->drag.icons->node, this->cursor->x, this->cursor->y);
    }

    bool handled = false;

    if(this->wm.handle && this->wm.callbacks.on_pointer_move){
        auto pevent = wm_create_pointer_event(this, 0, 0);

        handled = !this->wm.callbacks.on_pointer_move(&pevent);
            
        wm_destroy_pointer_event(&pevent);
    }

    if(handled){
        return;
    }

    auto [toplevel, input_on_surface] = utils::desktop_toplevel_at(this, this->cursor->x, this->cursor->y);

    if(input_on_surface.surface){
        wlr_seat_pointer_notify_enter(this->seat, input_on_surface.surface, input_on_surface.x, input_on_surface.y);
        wlr_seat_pointer_notify_motion(this->seat, time, input_on_surface.x,
            input_on_surface.y);
        wlr_seat_pointer_notify_frame(this->seat);
    } else {
        wlr_seat_pointer_clear_focus(this->seat);
        wlr_cursor_set_xcursor(this->cursor, this->cursor_mgr, "default");
    }
}

void handle_request_set_cursor_shape(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, cursor_shape_set);
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = 
        reinterpret_cast<struct wlr_cursor_shape_manager_v1_request_set_shape_event*>(data);

	struct wl_client *focused_client = NULL;
	struct wlr_surface *focused_surface = server->seat->pointer_state.focused_surface;

	if (focused_surface != NULL) {
		focused_client = wl_resource_get_client(focused_surface->resource);
	}

	if (focused_client == NULL || event->seat_client->client != focused_client) {
        wlr_log(WLR_DEBUG, "cant change cursor if not focused");
		return;
	}

	if (!(server->seat->capabilities & WL_SEAT_CAPABILITY_POINTER)) {
		return;
	}
    
    const char *image = wlr_cursor_shape_v1_name(event->shape);

	if (!image) {
		wlr_cursor_unset_image(server->cursor);
        return;
    }
    
	wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, image);
}

void yawc_server::create_cursor()
{
    wlr_log(WLR_DEBUG, "Creating cursor");

    this->cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(this->cursor, this->output_layout);

    this->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

    this->relative_pointer_manager = wlr_relative_pointer_manager_v1_create(this->wl_display);

    this->create_pointer_constraint();

    this->pointer_motion_listener.notify = +[](struct wl_listener* listener, void* data) {
        struct yawc_server* server = wl_container_of(listener, server, pointer_motion_listener);
        server->handle_pointer_motion(listener, data, false);
    };
    wl_signal_add(&this->cursor->events.motion, &this->pointer_motion_listener);

    this->pointer_motion_absolute_listener.notify = +[](struct wl_listener* listener, void* data) {
        struct yawc_server* server = wl_container_of(listener, server, pointer_motion_absolute_listener);
        server->handle_pointer_motion(listener, data, true);
    };
    wl_signal_add(&this->cursor->events.motion_absolute, &this->pointer_motion_absolute_listener);

    this->cursor_frame_listener.notify = +[](struct wl_listener* listener, void* data) {
        struct yawc_server* server = wl_container_of(listener, server, cursor_frame_listener);
        wlr_seat_pointer_notify_frame(server->seat);
    };
    wl_signal_add(&this->cursor->events.frame, &this->cursor_frame_listener);

    this->cursor_button_listener.notify = handle_cursor_button;
    wl_signal_add(&this->cursor->events.button, &this->cursor_button_listener);

    this->on_axis_cursor_motion.notify = handle_cursor_axis;
    wl_signal_add(&this->cursor->events.axis, &this->on_axis_cursor_motion);

    struct wlr_cursor_shape_manager_v1 *cursor_shape_manager =
		wlr_cursor_shape_manager_v1_create(this->wl_display, 1);
	this->cursor_shape_set.notify = handle_request_set_cursor_shape;
	wl_signal_add(&cursor_shape_manager->events.request_set_shape, &this->cursor_shape_set);
}

void on_pointer_device_destroy(struct wl_listener *listener, void *data){
    struct yawc_pointer *pointer = wl_container_of(listener, pointer, destroy);

    wl_list_remove(&pointer->destroy.link);
    wl_list_remove(&pointer->link);

    delete pointer;
}

yawc_pointer *yawc_server::handle_pointer(struct wlr_input_device *device){
    yawc_pointer *pointer = new yawc_pointer;
    pointer->wlr_device = device;

    pointer->destroy.notify = on_pointer_device_destroy;
    wl_signal_add(&device->events.destroy, &pointer->destroy);

    wl_list_insert(&this->pointers, &pointer->link);

    return pointer;
}

void yawc_server::load_pointer_cfg(yawc_pointer *pointer){
    yawc_pointer_config config;

    auto device = pointer->wlr_device;

	if(this->config->input_configs.count(device->name)){
		config = std::get<yawc_pointer_config>(this->config->input_configs[device->name]);
	} else{
		config = this->config->default_pointer_config;
	}

    if(!config.enabled){
        return;
    }

    if (!wlr_input_device_is_libinput(device)) {
        wlr_cursor_attach_input_device(this->cursor, device);
        return;
    }

    struct libinput_device *libinput_handle = wlr_libinput_get_device_handle(device);

    if (config.accel_profile.has_value()) {
        std::string name = *config.accel_profile;
        auto profile = LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;

        if (name == "adaptive") {
            profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
        } else if (name == "custom") {
            profile = LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM;
        } else if (name == "flat") {
            profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
        }

        if (libinput_device_config_accel_is_available(libinput_handle)) {
            libinput_device_config_accel_set_profile(libinput_handle, profile);
        }
    }

    if (config.accel_speed.has_value()) {
        if (libinput_device_config_accel_is_available(libinput_handle)) {
            libinput_device_config_accel_set_speed(libinput_handle, *config.accel_speed);
        }
    }

    if (libinput_device_config_tap_get_finger_count(libinput_handle) > 0) {
        if (config.tap_to_click.has_value()) {
            libinput_device_config_tap_set_enabled(libinput_handle,
                *config.tap_to_click ? LIBINPUT_CONFIG_TAP_ENABLED : LIBINPUT_CONFIG_TAP_DISABLED);
        }

        if (config.tap_and_drag.has_value()) {
            libinput_device_config_tap_set_drag_enabled(libinput_handle,
                *config.tap_and_drag ? LIBINPUT_CONFIG_DRAG_ENABLED : LIBINPUT_CONFIG_DRAG_DISABLED);
        }

        if (config.tap_drag_lock.has_value()) {
            libinput_device_config_tap_set_drag_lock_enabled(libinput_handle,
                *config.tap_drag_lock ? LIBINPUT_CONFIG_DRAG_LOCK_ENABLED : LIBINPUT_CONFIG_DRAG_LOCK_DISABLED);
        }

        if (config.tap_button_map.has_value()) {
            auto map = LIBINPUT_CONFIG_TAP_MAP_LRM;
            if (*config.tap_button_map == "lmr") {
                map = LIBINPUT_CONFIG_TAP_MAP_LMR;
            }
            libinput_device_config_tap_set_button_map(libinput_handle, map);
        }
    }

    if (config.left_handed.has_value()) {
        if (libinput_device_config_left_handed_is_available(libinput_handle)) {
            libinput_device_config_left_handed_set(libinput_handle,
                *config.left_handed);
        }
    }

    if (config.nat_scrolling.has_value()) {
        if (libinput_device_config_scroll_has_natural_scroll(libinput_handle)) {
            libinput_device_config_scroll_set_natural_scroll_enabled(libinput_handle,
                *config.nat_scrolling);
        }
    }

    if (config.scroll_method.has_value()) {
        auto method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
        std::string m = *config.scroll_method;
        
        if (m == "2fg") {
            method = LIBINPUT_CONFIG_SCROLL_2FG;
        } else if (m == "edge") {
            method = LIBINPUT_CONFIG_SCROLL_EDGE;
        } else if (m == "button") {
            method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
        } else if (m == "none") {
            method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
        }

        libinput_device_config_scroll_set_method(libinput_handle, method);
    }

    if (config.scroll_button.has_value()) {
        libinput_device_config_scroll_set_button(libinput_handle, *config.scroll_button);
    }

    if (config.scroll_button_lock.has_value()) {
        libinput_device_config_scroll_set_button_lock(libinput_handle,
            *config.scroll_button_lock ? LIBINPUT_CONFIG_SCROLL_BUTTON_LOCK_ENABLED : LIBINPUT_CONFIG_SCROLL_BUTTON_LOCK_DISABLED);
    }

    if (config.middle_emulation.has_value()) {
        if (libinput_device_config_middle_emulation_is_available(libinput_handle)) {
            libinput_device_config_middle_emulation_set_enabled(libinput_handle,
                *config.middle_emulation ? LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED : LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);
        }
    }

    if (config.click_method.has_value()) {
        auto method = LIBINPUT_CONFIG_CLICK_METHOD_NONE;

        if (*config.click_method == "button_areas") {
            method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
        } else if (*config.click_method == "clickfinger") {
            method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
        }
        
        libinput_device_config_click_set_method(libinput_handle, method);
    }

    if (config.calibration_matrix.has_value() && config.calibration_matrix->size() == 6) {
        if (libinput_device_config_calibration_has_matrix(libinput_handle)) {
            float matrix[6];
            std::copy(config.calibration_matrix->begin(), config.calibration_matrix->end(), matrix);
            libinput_device_config_calibration_set_matrix(libinput_handle, matrix);
        }
    }

    if (config.disable_w_typing.has_value()) {
        if (libinput_device_config_dwt_is_available(libinput_handle)) {
            libinput_device_config_dwt_set_enabled(libinput_handle, 
                *config.disable_w_typing ? LIBINPUT_CONFIG_DWT_ENABLED : LIBINPUT_CONFIG_DWT_DISABLED);
        }
    }
    
    if (config.disable_w_trackpointing.has_value()) {
        if (libinput_device_config_dwtp_is_available(libinput_handle)) {
            libinput_device_config_dwtp_set_enabled(libinput_handle,
                *config.disable_w_trackpointing ? LIBINPUT_CONFIG_DWTP_ENABLED : LIBINPUT_CONFIG_DWTP_DISABLED);
        }
    }

    wlr_cursor_attach_input_device(this->cursor, device);
}
