#include "scene_descriptor.hpp"

#include "server.hpp"

void scene_descriptor_destroy(struct wlr_scene_node *node){
	if(!node){
		return;
	}

	struct yawc_scene_descriptor *desc =
            static_cast<struct yawc_scene_descriptor*>(node->data);

	if(!desc){
		return;
	}
        
	node->data = nullptr;

    delete desc;
}

bool scene_descriptor_assign(struct wlr_scene_node *node,
		enum yawc_scene_descriptor_type type, void *data, uint32_t resize_edges) {

	struct yawc_scene_descriptor *desc = new yawc_scene_descriptor{};

	if (!desc) {
		wlr_log(WLR_DEBUG, "Could not allocate a scene descriptor");
		return false;
	}

	desc->type = type;
	desc->data = data;
	desc->resize_edges = resize_edges;

	node->data = desc;

	return true;
}

struct yawc_layer_surface *scene_descriptor_try_get(struct wlr_scene_node *node,
        enum yawc_scene_descriptor_type type) {

    if (!node) {
        return NULL;
    }

    struct yawc_scene_descriptor *desc = static_cast<struct yawc_scene_descriptor*>(node->data);

    if (!desc || desc->type != type) {
        return NULL;
    }

    return static_cast<struct yawc_layer_surface *>(desc->data);
}
