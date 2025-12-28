#include <wayland-server-core.h>

enum yawc_scene_descriptor_type {
	YAWC_SCENE_DESC_BUFFER_TIMER,
	YAWC_SCENE_DESC_NON_INTERACTIVE,
	YAWC_SCENE_DESC_CONTAINER,
	YAWC_SCENE_DESC_VIEW,
	YAWC_SCENE_DESC_LAYER_SHELL,
	YAWC_SCENE_DESC_POPUP,
	YAWC_SCENE_DESC_DRAG_ICON,
	YAWC_SCENE_DESC_RESIZE_GRIP,
};

struct yawc_scene_descriptor{
	enum yawc_scene_descriptor_type type;
	void *data;
	uint32_t resize_edges;
	struct wl_listener destroy;
};

bool scene_descriptor_assign(struct wlr_scene_node *node,
	enum yawc_scene_descriptor_type type, void *data, uint32_t resize_edges);

struct yawc_layer_surface *scene_descriptor_try_get(struct wlr_scene_node *node, enum yawc_scene_descriptor_type type);

void scene_descriptor_destroy(struct wlr_scene_node *node);

