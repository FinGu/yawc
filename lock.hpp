#include "toplevel.hpp"

struct yawc_session_lock_output{
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *background;

	struct yawc_output *output;

	struct yawc_server *server;

	struct wl_list link;

	struct wl_listener destroy;

	struct wlr_session_lock_surface_v1 *surface;

	struct wl_listener surface_destroy;
	struct wl_listener surface_map;
};

void arrange_locks(struct yawc_server *sv);

struct yawc_session_lock_output *session_lock_output_create(struct yawc_server *sv, struct yawc_output *output);
