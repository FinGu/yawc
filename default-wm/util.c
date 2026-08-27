#include "util.h"

#include "config.h"
#include "wm_api.h"

#include <math.h>

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

wm_box_t maximize_window(wm_toplevel *toplevel){
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

	return output_geometry;
}

wm_box_t unmaximize_window(wm_toplevel *toplevel){
	double cursor_x, cursor_y;
	wm_get_cursor_coords(&cursor_x, &cursor_y);

    wm_box_t max_geo = wm_get_toplevel_geometry(toplevel);
    wm_box_t restore_geo = wm_get_last_toplevel_geometry(toplevel);

    double ratio_x = (cursor_x - max_geo.x) / (double)max_geo.width;
	double ratio_y = (cursor_y - max_geo.y) / (double)max_geo.height;

	if(ratio_x > 1){
		ratio_x -= 1; // to handle the edge case of our right side gestures
					  // can this ever go over 2? Im honestly not sure
	}

	restore_geo.x = cursor_x - (ratio_x * restore_geo.width);
	restore_geo.y = cursor_y - (ratio_y * restore_geo.height);

	if(!wm_toplevel_is_csd(toplevel)){
		//decent enough
		restore_geo.y += DECORATION_HEIGHT;
	}

	wm_set_toplevel_maximized(toplevel, false);
	wm_set_toplevel_geometry(toplevel, restore_geo);

	return restore_geo;
}

void fullscreen_window(wm_toplevel *toplevel, wm_output *output){
	bool should_unref = false;

	if(!output){
		output = wm_get_output_of_toplevel(toplevel);		
		should_unref = true;
	}

	wm_box_t output_box = wm_get_output_geometry(output);

	wm_set_toplevel_fullscreen(toplevel, true);
	wm_set_toplevel_geometry(toplevel, output_box);
	wm_change_toplevel_layer(toplevel, WM_LAYER_ABOVE);

	if(should_unref){
		wm_unref_output(output);
	}
}

void unfullscreen_window(wm_toplevel *toplevel){
	wm_box_t restore_geo = wm_get_last_toplevel_geometry(toplevel);

	wm_set_toplevel_fullscreen(toplevel, false);
	wm_set_toplevel_geometry(toplevel, restore_geo);
	wm_change_toplevel_layer(toplevel, WM_LAYER_NORMAL);
}

double get_time_diff(struct timespec end, struct timespec start) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

void hide_and_repair_focus(wm_toplevel *toplevel){
	//we only focus the next toplevel in case the one we're hiding is currently being used ( allow for show desktop button )
	bool active = wm_toplevel_is_focused(toplevel);

	wm_hide_toplevel(toplevel);

	if(!active){
		return;
	}

	focus_next_toplevel(toplevel);
}

void focus_next_toplevel(wm_toplevel *toplevel){
	wm_toplevel *next = wm_get_next_toplevel(toplevel);

	if(!next){
		return;	
	}

	wm_unhide_toplevel(next);
	wm_focus_toplevel(next);

	wm_unref_toplevel(next);
}


