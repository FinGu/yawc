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

	desc->parent = nullptr;
	desc->data = nullptr;

	node->data = nullptr;

    delete desc;
}

bool scene_descriptor_assign(struct wlr_scene_node *node,
		enum yawc_scene_descriptor_type type, void *parent, void *data) {

	struct yawc_scene_descriptor *desc = new yawc_scene_descriptor{};

	if (!desc) {
		wlr_log(WLR_DEBUG, "Could not allocate a scene descriptor");
		return false;
	}

	desc->type = type;

	desc->parent = parent;
	desc->data = data;

	node->data = desc;

	return true;
}

struct yawc_scene_descriptor *scene_descriptor_try_get(struct wlr_scene_node *node,
        enum yawc_scene_descriptor_type type) {

    if (!node) {
        return nullptr;
    }

    struct yawc_scene_descriptor *desc = static_cast<struct yawc_scene_descriptor*>(node->data);

    if (!desc || desc->type != type) {
        return nullptr;
    }

	return desc;
}
