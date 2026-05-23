#include "task_switcher.h"
#include "config.h"

#include <string.h>

void draw_switcher_element(struct task_switcher_data *wdata, wm_toplevel *cur){
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

void draw_task_switcher(void *data){
    struct task_switcher_data *ts_data = data;
    struct nk_context *ctx = &ts_data->ctx;

    wm_box_t geo = wm_get_buffer_geometry(ts_data->buffer);

    nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_hide());
    nk_style_push_color(ctx, &ctx->style.window.border_color, nk_rgba(0,0,0,0));
    nk_style_push_float(ctx, &ctx->style.window.border, 0.0f);
    nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(0,0));

    nk_style_push_color(ctx, &ctx->style.window.group_border_color, nk_rgba(0,0,0,0));
    nk_style_push_float(ctx, &ctx->style.window.group_border, 0.0f);

    if (ts_data->toplevel_amount > 0) {
        ts_data->current_index = (ts_data->current_index + 1) % ts_data->toplevel_amount;
    
        ts_data->selected = ts_data->toplevels[ts_data->current_index];

        wm_focus_toplevel(ts_data->selected);
        wm_unhide_toplevel(ts_data->selected);
    } else{
        ts_data->selected = NULL;
    }

    if (nk_begin(ctx, "windowlist", nk_rect(0, 0, geo.width, geo.height), 
        NK_WINDOW_NO_SCROLLBAR)) {

        nk_layout_row_dynamic(ctx, geo.height + 50, 1);

        if (nk_group_begin(ctx, "listgroup", NK_WINDOW_BORDER)) {
            for(int i = 0; i < ts_data->toplevel_amount; ++i){
                draw_switcher_element(ts_data, ts_data->toplevels[i]);
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
                            
    nk_wm_render(NK_ANTI_ALIASING_ON, MAX_NUKLEAR_VERTEX_MEMORY, MAX_NUKLEAR_ELEMENT_MEMORY, geo.width, geo.height, ctx);
}

void destroy_task_switcher(struct task_switcher_data *ts_data){
    if(!ts_data->active){
        return;
    }

    wm_buffer *buf = wm_unattach_overlay(WINDOW_LIST_OVERLAY);

    if(buf){
        wm_destroy_buffer(buf);
    }

    wm_unref_toplevels(ts_data->toplevels, ts_data->toplevel_amount);
    ts_data->buffer = NULL;
    ts_data->toplevels = NULL;
    ts_data->toplevel_amount = 0;
    ts_data->active = false;
}

wm_buffer *create_task_switcher(struct task_switcher_data *ts_data){
    if(!ts_data->initialized){
        ts_data->initialized = true;
        ts_data->ctx = nk_wm_ctx_create();
    }

    size_t toplevel_amount = 0;
    wm_toplevel **toplevels = wm_get_toplevels(&toplevel_amount);

    wm_output *output = wm_get_focused_output();
    wm_box_t output_geometry = wm_get_output_geometry(output);
    wm_unref_output(output);

    int32_t width = output_geometry.width / 4;
    int32_t height = (WINDOW_LIST_ENTRY_HEIGHT+7) * toplevel_amount;

    ts_data->active = true;

    wm_buffer *buf = wm_create_buffer(width, height, false);

    if(!ts_data->buffer){
        ts_data->buffer = buf; 

        ts_data->toplevels = toplevels;
        ts_data->toplevel_amount = toplevel_amount;

        ts_data->current_index = 0;
    } else{
        if(toplevels){
            wm_unref_toplevels(toplevels, toplevel_amount);
        }

        ts_data->buffer = buf;
    }
    
    wm_buffer *old_buffer = wm_attach_overlay(WINDOW_LIST_OVERLAY, ts_data->buffer, 
            output_geometry.x + (output_geometry.width / 2 - width / 2), 
            output_geometry.y + (output_geometry.height / 2 - height / 2));

    if(old_buffer){
        wm_plugin_log("Destroying old alt-tab buffer");
        wm_destroy_buffer(old_buffer);
    }

    wm_render_fn_to_buffer(ts_data->buffer, draw_task_switcher, ts_data);

    return ts_data->buffer;
}
