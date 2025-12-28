#include "../scene_descriptor.hpp"

struct yawc_layer_surface{
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
	struct wl_listener destroy;
	struct wl_listener new_popup;

	bool mapped;

	struct wl_list link;

	struct wlr_scene_layer_surface_v1 *scene;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wlr_scene_tree *tree;
	struct yawc_output *output;
};

struct yawc_layer_popup {
	struct wlr_xdg_popup *wlr_popup;
	struct wlr_scene_tree *scene;

	struct yawc_layer_surface *toplevel;

	struct wl_listener destroy;

	struct wl_listener new_popup;
	struct wl_listener commit;
	struct wl_listener reposition;
};

void arrange_layers(struct yawc_output *output);
