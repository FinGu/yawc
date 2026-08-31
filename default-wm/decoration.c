#include "decoration.h"
#include "config.h"
#include "util.h"

#define NK_IMPLEMENTATION
#include "nuklear_wm_gl2.h"

void draw_decoration(void *data){
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
		hide_and_repair_focus(toplevel);
        goto end;
    }

    if(nk_window_is_hidden(&wd->ctx, id)){
        wm_close_toplevel(toplevel);
        goto end;
    }

end:
    wm_unref_toplevel(toplevel);

    nk_wm_render(NK_ANTI_ALIASING_OFF, MAX_NUKLEAR_VERTEX_MEMORY, MAX_NUKLEAR_ELEMENT_MEMORY, geo.width, geo.height, &wd->ctx);
    nk_clear(&wd->ctx); //needs to be cleared to reset the input events so that minimize isn't triggered twice
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
    if(wm_is_toplevel_csd(toplevel)){ //is client side decoration
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

    wm_attach_toplevel_buffer(toplevel, DECORATION_NAME, buffer, geometry.x, geometry.y);

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

        wm_render_fn_to_buffer(buffer, draw_decoration, window_data);
        //render to a buffer, context needs to be changed to opengl's

        return buffer;
    }

    struct window_data *wdata = alloc_window_data(buffer);
    
    wm_attach_toplevel_state(toplevel, wdata); //we save the state

    wm_render_fn_to_buffer(buffer, draw_decoration, wdata);

    return buffer;
}

