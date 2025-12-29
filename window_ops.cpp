#include "window_ops.hpp"

#include "wm_defs.hpp"

#include "utils.hpp"

#include <cmath>

void begin_move(yawc_toplevel *toplevel)
{
    auto *server = toplevel->server;

    server->grabbed_toplevel = toplevel;
    server->current_mouse_operation = MOVING;

    //this needs to be revised, if i have a node that's not the root (the decoration) with a y of -30
    //we will stick the cursor to the actual window and not the decoration 
    int win_ax = 0.0, win_ay = 0.0;
    if (toplevel->scene_tree) {
        wlr_scene_node_coords(&toplevel->scene_tree->node, &win_ax, &win_ay);
    } else {
        win_ax = toplevel->scene_tree ? toplevel->scene_tree->node.x : 0;
        win_ay = toplevel->scene_tree ? toplevel->scene_tree->node.y : 0;
    }

    server->grabbed_mov_x = server->cursor->x - win_ax;
    server->grabbed_mov_y = server->cursor->y - win_ay;
}

void begin_resize(yawc_toplevel *toplevel, uint32_t edges)
{
    auto *server = toplevel->server;

    server->grabbed_toplevel = toplevel;
    server->current_mouse_operation = RESIZING;
    server->resize_edges = edges;

    struct wlr_box geo_box = toplevel->xdg_toplevel->base->geometry;

    int win_ax = 0, win_ay = 0;
    if (toplevel->scene_tree) {
        wlr_scene_node_coords(&toplevel->scene_tree->node, &win_ax, &win_ay);
    }

    double border_x = win_ax + geo_box.x + ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
    double border_y = win_ay + geo_box.y + ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);

    server->grabbed_res_x = server->cursor->x - border_x;
    server->grabbed_res_y = server->cursor->y - border_y;

    server->grabbed_geo_box = geo_box;

    server->grabbed_geo_box.x += win_ax;
    server->grabbed_geo_box.y += win_ay;
}

void yawc_toplevel::set_fullscreen(bool enable){
    if (enable) {
        struct yawc_output *chosen_output;
        wlr_output *requested_output = this->xdg_toplevel->requested.fullscreen_output;

        this->save_state();

        if(requested_output){
            chosen_output = static_cast<yawc_output*>(requested_output->data);
        } else{
            chosen_output = utils::get_output_of_toplevel(this);
        }

        struct wlr_box output_box;
        wlr_output_layout_get_box(this->server->output_layout, 
                                  chosen_output->wlr_output, &output_box);

        wlr_scene_node_set_position(&this->scene_tree->node, output_box.x, output_box.y);
        wlr_xdg_toplevel_set_size(this->xdg_toplevel, output_box.width, output_box.height);

        wlr_scene_node_reparent(&this->scene_tree->node, this->server->layers.fullscreen);

        this->fullscreen = true;
    } else {
        wlr_scene_node_reparent(&this->scene_tree->node, this->server->layers.normal);

        this->reset_state();

        this->fullscreen = false; 
    }

    wlr_xdg_toplevel_set_fullscreen(this->xdg_toplevel, enable);

    if(this->foreign_handle){ //can be called at creation
        wlr_foreign_toplevel_handle_v1_set_fullscreen(this->foreign_handle, enable);
    }

    this->send_geometry_update();
}

void yawc_toplevel::default_set_maximized(bool enable){
    struct yawc_output* output = utils::get_output_of_toplevel(this);
    
    struct wlr_box usable_box = utils::get_usable_area_of_output(output);

    this->maximized = enable;
    if(this->foreign_handle){ 
        wlr_foreign_toplevel_handle_v1_set_maximized(this->foreign_handle, enable);
    }

    wlr_xdg_toplevel_set_maximized(this->xdg_toplevel, enable);

    if(enable){
        wlr_scene_node_set_position(&this->scene_tree->node, usable_box.x, usable_box.y);
        wlr_xdg_toplevel_set_size(this->xdg_toplevel, usable_box.width, usable_box.height);

        return;
    }

    int lx, ly;
    wlr_scene_node_coords(&this->scene_tree->node, &lx, &ly);

    auto old_geo = this->xdg_toplevel->base->geometry;

    double ratio_x = (server->cursor->x - lx) / (double)old_geo.width;

    auto new_geo = this->reset_state();

    int new_x = this->server->cursor->x - (ratio_x * new_geo.width);
    int new_y = this->server->cursor->y;

    wlr_scene_node_set_position(&this->scene_tree->node, new_x, new_y);
}

void yawc_toplevel::default_set_minimized(bool enable){
    wlr_scene_node_set_enabled(&this->scene_tree->node, !enable);

    if(this->foreign_handle){
        wlr_foreign_toplevel_handle_v1_set_minimized(this->foreign_handle, enable);
    }

    if(enable && !wl_list_empty(&server->toplevels)){
        struct yawc_toplevel *new_focus = nullptr;
        struct yawc_toplevel *iter_toplevel;

        wl_list_for_each(iter_toplevel, &server->toplevels, link){
            if(this != iter_toplevel && iter_toplevel->mapped && !iter_toplevel->hidden){
                new_focus = iter_toplevel;
                break;
            }    
        }

        if(new_focus){
            utils::focus_toplevel(new_focus);
        }
    }

    this->hidden = enable;
}

