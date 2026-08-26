#include "requests.h"

#include "util.h"
#include "wm_api.h"

void on_toplevel_move_request(wm_toplevel_request_event_t *event){
	wm_toplevel *toplevel = event->toplevel;

	if(wm_toplevel_is_fullscreen(toplevel)){
		return;
	}

	if(wm_toplevel_is_maximized(toplevel)){
		wm_box_t result = unmaximize_window(toplevel);

		wm_begin_move_with_coords(toplevel, result.x, result.y);

		return;
	}

	wm_begin_move(toplevel);
}

void on_toplevel_resize_request(wm_toplevel_request_event_t *event){
	wm_toplevel *toplevel = event->toplevel;
	wm_resize_request_payload *payload = event->data;

	wm_begin_resize(toplevel, payload->edges);
}

void on_toplevel_maximize_request(wm_toplevel_request_event_t *event){
	wm_toplevel *toplevel = event->toplevel;
	wm_toggle_request_payload *payload = event->data;

	if(payload->state){
		maximize_window(toplevel);
	} else{
		unmaximize_window(toplevel);
	}
}

void on_toplevel_fullscreen_request(wm_toplevel_request_event_t *event){
	wm_toplevel *toplevel = event->toplevel;
	wm_fullscreen_request_payload *payload = event->data;

	if(payload->state){
		fullscreen_window(toplevel, payload->requested_output);
	} else{
		unfullscreen_window(toplevel);
	}
}

void on_toplevel_minimize_request(wm_toplevel_request_event_t *event){
	wm_toplevel *toplevel = event->toplevel;
	wm_toggle_request_payload *payload = event->data;

	if(!payload->state){
		wm_unhide_toplevel(toplevel);
		return;
	}

	hide_and_repair_focus(toplevel);
}

void on_toplevel_activate_request(wm_toplevel_request_event_t *event){
	wm_toplevel *toplevel = event->toplevel;

	wm_unhide_toplevel(toplevel);
	wm_focus_toplevel(toplevel);
}

void on_toplevel_close_request(wm_toplevel_request_event_t *event){
	wm_toplevel *toplevel = event->toplevel;

	wm_close_toplevel(toplevel);
}

