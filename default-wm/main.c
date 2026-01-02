#include "nuklear_wm_gl2.h"
#include "wm_api.h"

#define NK_IMPLEMENTATION
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "nuklear.h"

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

#define DOUBLE_CLICK_THRESHOLD 0.5
#define COORDS_THRESHOLD 5

#define DECORATION_NAME "decoration"
#define DECORATION_HEIGHT 30
#define RESIZE_MARGIN 10

#define WINDOW_LIST_OVERLAY "alttab"
#define WINDOW_LIST_ENTRY_HEIGHT 30

#include <time.h>

struct window_data{
    wm_buffer *buffer;
    struct nk_context ctx;
    
    struct timespec last_left_click; 
    int last_click_x;
    int last_click_y;
};

struct window_data *alloc_window_data(wm_buffer *buf){
    struct window_data *wdata = calloc(1, sizeof(struct window_data));
    
    wdata->buffer = buf;

    wdata->ctx = nk_wm_ctx_create();

    return wdata;
}

void free_window_data(struct window_data *data){
    wm_destroy_buffer(data->buffer);

    nk_wm_ctx_destroy(&data->ctx);

    free(data);
}

void draw_titlebar(void *data){
    struct window_data *wd = data;

    wm_box_t geo = wm_get_buffer_geometry(wd->buffer);

    wm_toplevel *toplevel = wm_get_toplevel_of_buffer(wd->buffer);

    if(!toplevel){
        return;
    }

    char id[32] = {0};
    nk_itoa(id, wm_get_toplevel_id(toplevel));
    const char *title = wm_get_toplevel_title(toplevel);

    if(nk_begin_titled(&wd->ctx, id, title, nk_rect(0, 0, geo.width, geo.height), NK_WINDOW_BORDER|
                NK_WINDOW_MINIMIZABLE|NK_WINDOW_CLOSABLE|NK_WINDOW_TITLE)){}

    nk_end(&wd->ctx);

    if(nk_window_is_collapsed(&wd->ctx, id)){
        wm_hide_toplevel(toplevel);
        goto end;
    }

    if(nk_window_is_hidden(&wd->ctx, id)){
        wm_close_toplevel(toplevel);
        goto end;
    }

end:
    wm_unref_toplevel(toplevel);

    nk_wm_render(NK_ANTI_ALIASING_OFF, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY, geo.width, geo.height, &wd->ctx);
    nk_clear(&wd->ctx); //needs to be cleared to reset the input events so that minimize isn't triggered twice
}

struct window_list_data{
    struct nk_context ctx;
    wm_toplevel *selected;

    wm_toplevel **toplevels;
    size_t toplevel_amount;

    int current_index;

    wm_buffer *buffer;
    bool initialized;

    bool active;
}; 

static struct window_list_data window_list_data = {.initialized = false};

void draw_window_element(struct window_list_data *wdata, wm_toplevel *cur){
    struct nk_context *ctx = &wdata->ctx;
    const char *title = wm_get_toplevel_title(cur);

    if (!title || strlen(title) == 0) {
        title = "Untitled Window";
    }

    int selected = wm_get_toplevel_id(cur) == wm_get_toplevel_id(wdata->selected);

    nk_layout_row_dynamic(ctx, WINDOW_LIST_ENTRY_HEIGHT, 1);

    if (selected) {
        struct nk_style_item blue = nk_style_item_color(nk_rgb(0, 122, 204));
        
        nk_style_push_style_item(ctx, &ctx->style.button.normal, blue);
        nk_style_push_style_item(ctx, &ctx->style.button.hover, blue);
        nk_style_push_style_item(ctx, &ctx->style.button.active, blue);
        nk_style_push_color(ctx, &ctx->style.button.text_normal, nk_rgb(255, 255, 255));
        nk_style_push_color(ctx, &ctx->style.button.text_hover, nk_rgb(255, 255, 255));
    
        nk_button_label(ctx, title);
        
        nk_style_pop_color(ctx);
        nk_style_pop_color(ctx);
        nk_style_pop_style_item(ctx);
        nk_style_pop_style_item(ctx);
        nk_style_pop_style_item(ctx);
    } else {
        struct nk_style_item clear = nk_style_item_color(nk_rgba(40, 40, 40, 255));

        nk_style_push_style_item(ctx, &ctx->style.button.normal, clear);
        nk_style_push_style_item(ctx, &ctx->style.button.hover, clear);
        nk_style_push_style_item(ctx, &ctx->style.button.active, clear);

        nk_button_label(ctx, title);

        nk_style_pop_style_item(ctx);
        nk_style_pop_style_item(ctx);
        nk_style_pop_style_item(ctx);
    }
}

void draw_window_list(void *data){
    struct window_list_data *wdata = data;
    struct nk_context *ctx = &wdata->ctx;

    wm_box_t geo = wm_get_buffer_geometry(wdata->buffer);

    nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_hide());
    nk_style_push_color(ctx, &ctx->style.window.border_color, nk_rgba(0,0,0,0));
    nk_style_push_float(ctx, &ctx->style.window.border, 0.0f);
    nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(0,0));

    nk_style_push_color(ctx, &ctx->style.window.group_border_color, nk_rgba(0,0,0,0));
    nk_style_push_float(ctx, &ctx->style.window.group_border, 0.0f);

    if (wdata->toplevel_amount > 0) {
        wdata->current_index = (wdata->current_index + 1) % wdata->toplevel_amount;
    
        wdata->selected = wdata->toplevels[wdata->current_index];

        wm_focus_toplevel(wdata->selected);
        wm_unhide_toplevel(wdata->selected);
    } else{
        wdata->selected = NULL;
    }

    if (nk_begin(ctx, "windowlist", nk_rect(0, 0, geo.width, geo.height), 
        NK_WINDOW_NO_SCROLLBAR)) {

        nk_layout_row_dynamic(ctx, geo.height + 50, 1);

        if (nk_group_begin(ctx, "listgroup", NK_WINDOW_BORDER)) {
            for(int i = 0; i < wdata->toplevel_amount; ++i){
                draw_window_element(&window_list_data, wdata->toplevels[i]);
            }

            nk_group_end(ctx);
        }
    }

    nk_end(ctx);

    nk_style_pop_float(ctx);
    nk_style_pop_color(ctx);
    nk_style_pop_vec2(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_color(ctx);
    nk_style_pop_style_item(ctx);
                            
    nk_wm_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY, geo.width, geo.height, ctx);
}

void destroy_window_list(){
    if(!window_list_data.active){
        return;
    }

    wm_buffer *buf = wm_unattach_overlay(WINDOW_LIST_OVERLAY);

    if(buf){
        wm_destroy_buffer(buf);
    }

    wm_unref_toplevels(window_list_data.toplevels, window_list_data.toplevel_amount);
    window_list_data.buffer = NULL;
    window_list_data.toplevels = NULL;
    window_list_data.toplevel_amount = 0;
    window_list_data.active = false;
}

wm_buffer *create_window_list(){
    if(!window_list_data.initialized){
        window_list_data.initialized = true;
        window_list_data.ctx = nk_wm_ctx_create();
    }

    size_t toplevel_amount = 0;
    wm_toplevel **toplevels = wm_get_toplevels(&toplevel_amount);

    wm_output *output = wm_get_focused_output();
    wm_box_t output_geometry = wm_get_output_geometry(output);
    wm_unref_output(output);

    int32_t width = output_geometry.width / 4;
    int32_t height = (WINDOW_LIST_ENTRY_HEIGHT+7) * toplevel_amount;

    window_list_data.active = true;

    wm_buffer *buf = wm_create_buffer(width, height, false);

    if(!window_list_data.buffer){
        window_list_data.buffer = buf; 

        window_list_data.toplevels = toplevels;
        window_list_data.toplevel_amount = toplevel_amount;

        window_list_data.current_index = 0;
    } else{
        if(toplevels){
            wm_unref_toplevels(toplevels, toplevel_amount);
        }

        window_list_data.buffer = buf;
    }
    
    wm_buffer *old_buffer = wm_attach_overlay(WINDOW_LIST_OVERLAY, window_list_data.buffer, 
            output_geometry.x + (output_geometry.width / 2 - width / 2), 
            output_geometry.y + (output_geometry.height / 2 - height / 2));

    if(old_buffer){
        wm_plugin_log("Destroying old alt-tab buffer");
        wm_destroy_buffer(old_buffer);
    }

    wm_render_fn_to_buffer(window_list_data.buffer, draw_window_list, &window_list_data);

    return window_list_data.buffer;
}

wm_grip_visual grip_callback(wm_toplevel *toplevel, 
        int width, 
        int height, 
        uint32_t edge_bits,
        void *user_data){
    wm_grip_visual out = {};

    out.type = WM_GRIP_VISUAL_COLOR;
    memset(out.color, 0, sizeof(float)*4); //let's make invisible buffers!

    return out;
}

wm_buffer *create_decoration(wm_toplevel *toplevel, wm_box_t geometry) {
    if(wm_toplevel_is_csd(toplevel)){ //is client side decoration
        return NULL;
    }

    geometry.x = 0;
    geometry.y = -DECORATION_HEIGHT;

    uint32_t old_height = geometry.height;
    geometry.height = DECORATION_HEIGHT;
    //allocate some space for the decoration

    wm_buffer *buffer = wm_create_buffer(geometry.width, geometry.height, false);

    if(!buffer){
        wm_plugin_log("Failed to create buffer");
        return NULL;
    }

    wm_toplevel_attach_buffer(toplevel, DECORATION_NAME, buffer, geometry.x, geometry.y);

    wm_configure_toplevel_resize_grips(toplevel, 0, -DECORATION_HEIGHT, 
            geometry.width, old_height + DECORATION_HEIGHT, 
            RESIZE_MARGIN,
            grip_callback,
            NULL); //set up a few small boxes around the window for resizing and the like

    struct window_data* window_data = wm_get_toplevel_state(toplevel); //check if decoration already exists

    if(window_data){
        wm_plugin_log("Destroying old window data");

        wm_destroy_buffer(window_data->buffer);
        window_data->buffer = buffer;

        wm_render_fn_to_buffer(buffer, draw_titlebar, window_data);
        //render to a buffer, context needs to be changed to opengl's

        return buffer;
    }

    struct window_data *wdata = alloc_window_data(buffer);
    
    wm_toplevel_attach_state(toplevel, wdata); //we save the state

    wm_render_fn_to_buffer(buffer, draw_titlebar, wdata);

    return buffer;
}

wm_box_t center_window(wm_toplevel *toplevel){
    wm_box_t toplevel_geometry = wm_get_toplevel_geometry(toplevel);

    wm_output *cur_output = wm_get_output_of_toplevel(toplevel);

    if(!cur_output){
        cur_output = wm_get_focused_output();
    }

    wm_box_t output_geometry = wm_get_output_usable_area(cur_output);

    int target_x = output_geometry.x + (output_geometry.width - toplevel_geometry.width) / 2;
    int target_y = output_geometry.y + (output_geometry.height - toplevel_geometry.height) / 2;

    int max_x = output_geometry.x + output_geometry.width - toplevel_geometry.width;
    int max_y = output_geometry.y + output_geometry.height - toplevel_geometry.height;

    if (target_x > max_x) {
        target_x = max_x;
    }

    if (target_x < output_geometry.x) {
        target_x = output_geometry.x;
    }

    if (target_y > max_y) {
        target_y = max_y;
    }

    if (target_y < output_geometry.y) {
        target_y = output_geometry.y;
    }

    wm_set_toplevel_position(toplevel, target_x, target_y);

    wm_unref_output(cur_output);

    return output_geometry;
}

void on_toplevel_geometry(wm_toplevel *toplevel, wm_box_t last_geo, wm_box_t new_box){
    if(last_geo.width == new_box.width && last_geo.height == new_box.height){
        return;
    }
    
    wm_plugin_log("On toplevel new geometry (resize), last_geo: %i %i %i %i, new_geo: %i %i %i %i", 
            last_geo.x, last_geo.y, last_geo.width, last_geo.height, 
            new_box.x, new_box.y, new_box.width, new_box.height);

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

    destroy_window_list();

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
        destroy_window_list();
        return true;
    }

    if (!event->pressed) {
        return true;
    }

    if(event->keysym != XKB_KEY_Tab && event->keysym != XKB_KEY_f){
        return true;
    }

    create_window_list();

    return false; //do we want to send the event to (a/the) window? true or false
}

void maximize_window(wm_toplevel *toplevel){
    wm_output *output = wm_get_output_of_toplevel(toplevel);
    wm_box_t output_geometry = wm_get_output_usable_area(output);

    wm_unref_output(output);

    //here we could also remove the boxes created by the resize grip function
    
    if(!wm_toplevel_is_csd(toplevel)){ //account for the decoration
        output_geometry.height -= DECORATION_HEIGHT;
        output_geometry.y = DECORATION_HEIGHT;
    }

    wm_set_toplevel_maximized(toplevel, true);
    wm_set_toplevel_geometry(toplevel, output_geometry);
}

void on_toplevel_map(wm_toplevel *toplevel){
    wm_plugin_log("Mapping toplevel");

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
    destroy_window_list(); //we stop drawing the window list if it's there

    wm_node_at_coords_t *coords = wm_try_get_node_at_coords(event->global_x, event->global_y);

    if(!coords){
        return true;
    }

    uint32_t edges = wm_try_get_resize_grip(coords->node, NULL); 
    //that returns us the edges that the pointer is on

    wm_unref_node_at_coords(coords);

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

double get_time_diff(struct timespec end, struct timespec start) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

void handle_click_gestures(wm_pointer_event_t *event, struct window_data *data, wm_toplevel *toplevel, uint32_t edges) {
    if (event->button != BTN_LEFT) {
        return;
    }

    if(!event->pressed){
        return;
    }

    if(edges != WM_RESIZE_EDGE_INVALID){
        wm_begin_resize(toplevel, edges);
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    double time_diff = get_time_diff(now, data->last_left_click);

    int dx = abs(data->last_click_x - (int)event->global_x);
    int dy = abs(data->last_click_y - (int)event->global_y);
        
    //check for double clicks within a time frame
    if (time_diff < DOUBLE_CLICK_THRESHOLD && dx < COORDS_THRESHOLD && dy < COORDS_THRESHOLD) {
        maximize_window(toplevel);
            
        data->last_left_click.tv_sec = 0; 
        wm_cancel_window_op();
    } else {
        if (wm_toplevel_is_maximized(toplevel)) {
            wm_box_t max_geo = wm_get_toplevel_geometry(toplevel);
            wm_box_t restore_geo = wm_restore_toplevel_geometry(toplevel);

            double ratio_x = (event->global_x - max_geo.x) / (double)max_geo.width;
            //double offset_y = event->global_y - max_geo.y;
    
            restore_geo.x = event->global_x - (ratio_x * restore_geo.width);
            restore_geo.y = event->global_y + (DECORATION_HEIGHT/2.);

            wm_set_toplevel_maximized(toplevel, false);

            wm_set_toplevel_geometry(toplevel, restore_geo);
        }

        data->last_left_click = now;
        data->last_click_x = (int)event->global_x;
        data->last_click_y = (int)event->global_y;

        wm_begin_move(toplevel);
    } 
}

bool on_pointer_button(wm_pointer_event_t *event){
    wm_toplevel *toplevel = NULL;
    uint32_t edges = WM_RESIZE_EDGE_INVALID;

    wm_node_at_coords_t *node_with_coords = wm_try_get_node_at_coords(event->global_x, event->global_y);

    if(!node_with_coords){
        return true;
    }

    wm_node *node = node_with_coords->node;

    bool pass_event_back = true;

    toplevel = wm_try_get_toplevel_from_node(node);
    
    if(toplevel){ //if is a window we focus it
        wm_focus_toplevel(toplevel);
        goto free_toplevel;
    }

    wm_buffer *buffer = wm_try_get_buffer_from_node(node);

    struct window_data *data;

    if(buffer){ //if it's a buffer ( could be any type of buffer )
        toplevel = wm_get_toplevel_of_buffer(buffer);

        if(!toplevel){ //if is not a toplevel buffer
            goto free_node;
        }

        data = wm_get_toplevel_state(toplevel);

        wm_focus_toplevel(toplevel);

        //handle clicking on what matters
        nk_input_begin(&data->ctx);
        nk_wm_handle_pointer_event(&data->ctx, event, node_with_coords->local_x, node_with_coords->local_y); 
        nk_input_end(&data->ctx);

        wm_render_fn_to_buffer(buffer, draw_titlebar, data);

        handle_click_gestures(event, data, toplevel, edges);

        pass_event_back = false;

        goto free_toplevel;
    } 
    else if((edges = wm_try_get_resize_grip(node, &toplevel)) != WM_RESIZE_EDGE_INVALID){
        //in case it's not a buffer, it's the resize grip
        
        data = wm_get_toplevel_state(toplevel);

        handle_click_gestures(event, data, toplevel, edges);

        pass_event_back = false;
        goto free_toplevel;
    }

free_toplevel:
    wm_unref_toplevel(toplevel);
free_node:
    wm_unref_node_at_coords(node_with_coords);
    return pass_event_back;
}

bool wm_register(wm_callbacks_t *cbs, void *user_data){
    wm_plugin_log("Initializing the example window manager");

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

    if(window_list_data.initialized){
        wm_buffer *old_buffer = wm_unattach_overlay(WINDOW_LIST_OVERLAY);

        wm_destroy_buffer(old_buffer);

        nk_wm_ctx_destroy(&window_list_data.ctx);
    }

    wm_foreach_toplevel(clear_each_toplevel, NULL);

    nk_wm_shutdown();
}
