#include "task_switcher.h"

#include "decoration.h"

#include "util.h"

#include "config.h"

#include <stdlib.h>
#include <math.h>

static struct task_switcher_data task_switcher = {.initialized = false};

void on_toplevel_geometry(wm_toplevel *toplevel, wm_box_t last_geo, wm_box_t new_box){
    if(last_geo.width == new_box.width && last_geo.height == new_box.height){
        return;
    }
    
    /*wm_plugin_log("On toplevel new geometry (resize), last_geo: %i %i %i %i, new_geo: %i %i %i %i", 
            last_geo.x, last_geo.y, last_geo.width, last_geo.height, 
            new_box.x, new_box.y, new_box.width, new_box.height);*/

    wm_output *toplevel_output = wm_get_output_of_toplevel(toplevel);

    if(!toplevel_output){
        return;
    }

    wm_box_t output_geo = wm_get_output_geometry(toplevel_output);
    wm_unref_output(toplevel_output);

    if(!wm_toplevel_is_csd(toplevel) 
            && !wm_toplevel_is_maximized(toplevel) 
            && !wm_toplevel_is_fullscreen(toplevel)
            && new_box.y - DECORATION_HEIGHT < output_geo.y){
        wm_set_toplevel_position(toplevel, new_box.x, output_geo.y + DECORATION_HEIGHT);
    }

    create_decoration(toplevel, new_box);
}

void on_toplevel_unmap(wm_toplevel *toplevel){
    wm_plugin_log("Unmapping toplevel");

    destroy_task_switcher(&task_switcher);

    struct window_data *wdata = wm_get_toplevel_state(toplevel);

    if(wdata){
        free_window_data(wdata);
        wm_toplevel_attach_state(toplevel, NULL);
    }
}

bool on_keyboard_key(wm_keyboard_event_t *event){
    //verifying if we're alt-tabbing
    bool alt_down = (event->modifiers & WM_MODIFIER_ALT);

    if(!alt_down){
        destroy_task_switcher(&task_switcher);
        return true;
    }

    if (!event->pressed) {
        return true;
    }

    if(event->keysym != XKB_KEY_Tab && event->keysym != XKB_KEY_f){
        return true;
    }

    create_task_switcher(&task_switcher);

    return false; //do we want to send the event to (a/the) window? true or false
}

void on_toplevel_map(wm_toplevel *toplevel){
    wm_focus_toplevel(toplevel);

    create_decoration(toplevel, wm_get_toplevel_geometry(toplevel));

    if(wm_toplevel_wants_fullscreen(toplevel)){
        wm_set_toplevel_fullscreen(toplevel, true);
    } else if(wm_toplevel_wants_maximize(toplevel)){
        maximize_window(toplevel);
    } else{
        center_window(toplevel);
    }
}

bool hover_cursor = false; 
bool on_pointer_move(wm_pointer_event_t *event){
    destroy_task_switcher(&task_switcher); //we stop drawing the window list if it's there

    wm_node node = {NULL};

    wm_try_get_node_at_coords(&node, event->global_x, event->global_y);

    if(!node.node){
        return true;
    }

    uint32_t edges = wm_try_get_resize_grip(&node, NULL); 
    //that returns us the edges that the pointer is on

    if(edges != WM_RESIZE_EDGE_INVALID){
        wm_set_cursor(wm_get_cursor_name_from_edges(edges));

        hover_cursor = true; 
        return false;
    } 

    if(hover_cursor){ //so we don't get an edge cursor after leaving the surface
        wm_set_cursor("default");
        hover_cursor = false;
    }

    return true;
}

void handle_pressed_gestures(wm_pointer_event_t *event, struct window_data *data, wm_toplevel *toplevel, uint32_t edges) {
    if(event->button != BTN_LEFT) {
        return;
    }

    if(!event->pressed){
        return;
    }

    if(edges != WM_RESIZE_EDGE_INVALID){
		if(wm_toplevel_is_fullscreen(toplevel)){ //overwatch seemingly leaves the buffer on top, with which i can interact
			return;
		}

        wm_begin_resize(toplevel, edges);
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    double time_diff = get_time_diff(now, data->last_left_click);

    int dx = abs(data->last_click_x - (int)event->global_x);
    int dy = abs(data->last_click_y - (int)event->global_y);
        
    //check for double clicks within a time frame
    if(time_diff < DOUBLE_CLICK_THRESHOLD && dx < COORDS_THRESHOLD && dy < COORDS_THRESHOLD) {
        data->last_left_click.tv_sec = 0; 

        maximize_window(toplevel);

        wm_cancel_window_op();
    } else {
        data->last_left_click = now;
        data->last_click_x = (int)event->global_x;
        data->last_click_y = (int)event->global_y;

        if(wm_toplevel_is_maximized(toplevel)){
            restore_maximized_window(toplevel, event->global_x, event->global_y);
        }

        wm_begin_move(toplevel);
    } 
}

void handle_border_gesture(wm_pointer_event_t *event, wm_toplevel *cur_toplevel){
    if(event->button != BTN_LEFT) {
        return;
    }

    if(event->pressed){
        return;
    }

    if(event->op != WM_MOVING){
        return;
    }

    wm_output *output = wm_get_output_of_toplevel(cur_toplevel);

    if(!output){
        return;
    }

    wm_box_t output_geo = wm_get_output_geometry(output);

    wm_unref_output(output);

    bool is_left   = fabs(output_geo.x - event->global_x) < COORDS_THRESHOLD;
    bool is_right  = fabs((output_geo.x + output_geo.width - 1) - event->global_x) < COORDS_THRESHOLD;
    bool is_top    = fabs(output_geo.y - event->global_y) < COORDS_THRESHOLD;
    bool is_bottom = fabs((output_geo.y + output_geo.height - 1) - event->global_y) < COORDS_THRESHOLD;

    if (!is_left && !is_right && !is_top && !is_bottom) {
        return;
    }

    wm_box_t new_geo = output_geo;
    
    if(!wm_toplevel_is_csd(cur_toplevel)){
        new_geo.y += DECORATION_HEIGHT;
        new_geo.height -= DECORATION_HEIGHT;
    }

    if (is_left) {
        new_geo.width /= 2;
    } else if (is_right) {
        new_geo.width /= 2;
        new_geo.x += new_geo.width; 
    }

    if (is_top) {
        new_geo.height /= 2;
    } else if (is_bottom) {
        new_geo.height /= 2;
        new_geo.y += new_geo.height;
    }

    wm_set_toplevel_maximized(cur_toplevel, true);
    wm_set_toplevel_geometry(cur_toplevel, new_geo);
}

bool on_pointer_button(wm_pointer_event_t *event){
    wm_node node = {NULL};

    wm_toplevel *toplevel = NULL;
    uint32_t edges = WM_RESIZE_EDGE_INVALID;

    wm_node_coords_t coords = wm_try_get_node_at_coords(&node, event->global_x, event->global_y);

    if(!node.node){
        return true;
    }

    bool pass_event_back = true;

    toplevel = wm_try_get_toplevel_from_node(&node);
    
    if(toplevel){ //if is a window we focus it
        if(event->pressed){
            wm_focus_toplevel(toplevel);
        } else{
            pass_event_back = false;
        }

        handle_border_gesture(event, toplevel);
        
        goto free_toplevel;
    }

    wm_buffer *buffer = wm_try_get_buffer_from_node(&node);

    struct window_data *data;

    if(buffer){ //if it's a buffer ( could be any type of buffer )
        toplevel = wm_get_toplevel_of_buffer(buffer);

        if(!toplevel){ //if is not a toplevel buffer
            return pass_event_back;
        }

        data = wm_get_toplevel_state(toplevel);

        wm_focus_toplevel(toplevel);

        //handle clicking on what matters
        nk_input_begin(&data->ctx);
        nk_wm_handle_pointer_event(&data->ctx, event, coords.local_x, coords.local_y); 
        nk_input_end(&data->ctx);

        wm_render_fn_to_buffer(buffer, draw_decoration, data);

        handle_pressed_gestures(event, data, toplevel, edges);
        handle_border_gesture(event, toplevel);

        pass_event_back = false;

        goto free_toplevel;
    } 
    else if((edges = wm_try_get_resize_grip(&node, &toplevel)) != WM_RESIZE_EDGE_INVALID){
        //in case it's not a buffer, it's the resize grip
        
        data = wm_get_toplevel_state(toplevel);

        handle_pressed_gestures(event, data, toplevel, edges);

        pass_event_back = false;
        goto free_toplevel;
    }

free_toplevel:
    wm_unref_toplevel(toplevel);
    return pass_event_back;
}

bool wm_register(wm_callbacks_t *cbs, void *user_data){
    wm_plugin_log("Initializing the default window manager");

    cbs->on_pointer_button = on_pointer_button;
    cbs->on_map = on_toplevel_map;
    cbs->on_unmap = on_toplevel_unmap;
    cbs->on_pointer_move = on_pointer_move;
    cbs->on_geometry = on_toplevel_geometry;
    cbs->on_key = on_keyboard_key;

    cbs->on_toplevel_request_event = NULL; 
    // the compositor by default accepts and handle all requests in a floating way

    //we need to run this with an opengl context
    wm_render_fn_to_buffer(NULL, nk_wm_init, NULL);

    return true;
}

void clear_each_toplevel(wm_toplevel *t, void *data){
    on_toplevel_unmap(t);
}

void wm_unregister(){
    wm_plugin_log("Uninitializing the example window manager");

    if(task_switcher.initialized){
        wm_buffer *old_buffer = wm_unattach_overlay(WINDOW_LIST_OVERLAY);

        wm_destroy_buffer(old_buffer);

        nk_wm_ctx_destroy(&task_switcher.ctx);
    }

    wm_foreach_toplevel(clear_each_toplevel, NULL);

    nk_wm_shutdown();
}
