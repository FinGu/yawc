#include "util.h"

#include "config.h"

void center_window(wm_toplevel *toplevel){
    wm_box_t toplevel_geometry = wm_get_toplevel_geometry(toplevel);

    wm_output *cur_output = wm_get_output_of_toplevel(toplevel);

    if(!cur_output){
        cur_output = wm_get_focused_output();
    }

    wm_box_t output_geometry = wm_get_output_usable_area(cur_output);

    int decoration = (wm_toplevel_is_csd(toplevel) ? 0 : DECORATION_HEIGHT);

    int target_x = output_geometry.x + (output_geometry.width - toplevel_geometry.width) / 2;
    int target_y = output_geometry.y + (output_geometry.height - toplevel_geometry.height + decoration) / 2;

    int max_x = output_geometry.x + output_geometry.width - toplevel_geometry.width;
    int max_y = output_geometry.y + output_geometry.height - toplevel_geometry.height + decoration;

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
}

void maximize_window(wm_toplevel *toplevel){
    wm_output *output = wm_get_output_of_toplevel(toplevel);
    wm_box_t output_geometry = wm_get_output_usable_area(output);

    wm_unref_output(output);

    //here we could also remove the boxes created by the resize grip function
    
    if(!wm_toplevel_is_csd(toplevel)){ //account for the decoration
        output_geometry.height -= DECORATION_HEIGHT;
        output_geometry.y += DECORATION_HEIGHT;
    }

    wm_set_toplevel_maximized(toplevel, true);
    wm_set_toplevel_geometry(toplevel, output_geometry);
}

void restore_maximized_window(wm_toplevel *toplevel, double cursor_x, double cursor_y){
    wm_box_t max_geo = wm_get_toplevel_geometry(toplevel);
    wm_box_t restore_geo = wm_restore_toplevel_geometry(toplevel);

    double ratio_x;

    //when cursor_x is higher than the width we need to do the inverse
    if((cursor_x - max_geo.x) > max_geo.width){
        ratio_x = max_geo.width / (cursor_x - max_geo.x);
    } else{
        ratio_x = (cursor_x - max_geo.x) / max_geo.width;
    }
    
    restore_geo.x = cursor_x - (ratio_x * restore_geo.width);
    restore_geo.y = cursor_y + (DECORATION_HEIGHT/2.); // an estimate for y position is good enough

    wm_set_toplevel_maximized(toplevel, false);
    wm_set_toplevel_geometry(toplevel, restore_geo);
}

double get_time_diff(struct timespec end, struct timespec start) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

