#include "util.h"

void center_window(wm_toplevel *toplevel){
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
}
