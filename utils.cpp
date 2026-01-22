#include <fstream>
#include <linux/input-event-codes.h>
#include <unistd.h>

#include "utils.hpp"

#include "layer.hpp"
#include "toplevel.hpp"

std::tuple<yawc_toplevel*, yawc_input_on_surface>
utils::desktop_toplevel_at(yawc_server* server, double lx, double ly)
{
    double sx, sy;

    if(server->drag.icons){
        wlr_scene_node_set_enabled(&server->drag.icons->node, false);
    }

    struct wlr_scene_node* node = wlr_scene_node_at(&server->scene->tree.node, lx, ly, &sx, &sy);

    if(server->drag.icons){ // in case we're grabbing an icon we cant have the drag icon annoying us
        wlr_scene_node_set_enabled(&server->drag.icons->node, true);
    }

    if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
        return {};
    }

    struct wlr_scene_buffer* scene_buffer = wlr_scene_buffer_from_node(node);
    struct wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);

    if (!scene_surface) {
        return {};
    }

    yawc_input_on_surface out;

    out.surface = scene_surface->surface;
    out.x = sx;
    out.y = sy;

    struct wlr_scene_tree* tree = node->parent;
    while (tree != NULL && tree->node.data == NULL) {
        tree = tree->node.parent;
    }

    if (tree == NULL) {
        return std::make_tuple(nullptr, out);
    }
    
    auto desc = scene_descriptor_try_get(&tree->node, YAWC_SCENE_DESC_VIEW);

    if(!desc){
        return std::make_tuple(nullptr, out);
    }

    return std::make_tuple(static_cast<struct yawc_toplevel*>(desc->parent), out);
}

std::tuple<wlr_scene_node*, yawc_input_on_node>
utils::desktop_node_at(yawc_server* server, double lx, double ly)
{
    double sx, sy;

    struct wlr_scene_node* node = wlr_scene_node_at(&server->scene->tree.node, lx, ly, &sx, &sy);

    if (!node) {
        return std::make_tuple(nullptr, yawc_input_on_node{0,0});
    }

    yawc_input_on_node out;

    out.x = sx;
    out.y = sy;

    return std::make_tuple(node, out);
}

struct yawc_toplevel *utils::previous_toplevel(struct yawc_server *server){
    if(wl_list_empty(&server->toplevels)){
        return nullptr;
    }

    struct yawc_toplevel *prev_toplevel = wl_container_of(server->toplevels.next, prev_toplevel, link);
        
    if (!prev_toplevel->mapped) {
        return nullptr;
    }

    return prev_toplevel;
}

void utils::focus_toplevel(struct yawc_toplevel* toplevel)
{
    if (toplevel == nullptr) {
        return;
    }

    struct yawc_server* server = toplevel->server;

    struct wlr_seat* seat = server->seat;
    struct wlr_surface* prev_surface = seat->keyboard_state.focused_surface;

    struct wlr_surface* surface = toplevel->xdg_toplevel->base->surface;

    if (prev_surface == surface) {
        return;
    }

    server->constrain_cursor(nullptr);

    if (!wl_list_empty(&server->toplevels)) {
        struct yawc_toplevel *prev_toplevel = wl_container_of(server->toplevels.next, prev_toplevel, link);
        
        if (prev_toplevel != toplevel && prev_toplevel->mapped) {
            if(prev_toplevel->fullscreen){ //the idea is simple, if is a fullscreen window, alt tab should let other windows stack on top 
                wlr_scene_node_reparent(&prev_toplevel->scene_tree->node, server->layers.normal);
            } 

            prev_toplevel->activate(false);
        }
    }

    if(toplevel->scene_tree){
        wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);

        if(toplevel->fullscreen){
            wlr_scene_node_reparent(&toplevel->scene_tree->node, server->layers.fullscreen);
        }
    }

    wl_list_remove(&toplevel->link);
    wl_list_insert(&server->toplevels, &toplevel->link);
    
    toplevel->activate(true);

    server->set_focus_layer(nullptr);
    server->set_focus_surface(surface);

    struct wlr_pointer_constraint_v1 *req = wlr_pointer_constraints_v1_constraint_for_surface(
        server->pointer_constraints, surface, server->seat);

    server->constrain_cursor(req);
}

bool utils::toplevel_not_empty(struct yawc_toplevel* toplevel)
{
    if (!toplevel || !toplevel->scene_tree) {
        return false;
    }

    return toplevel->xdg_toplevel != nullptr && 
            toplevel->xdg_toplevel->base != nullptr;
}

struct yawc_output*
utils::get_output_of_toplevel(struct yawc_toplevel* toplevel)
{
	double closest_x, closest_y;
	struct wlr_output *output = nullptr;

    auto geometry = toplevel->xdg_toplevel->base->geometry;

    int global_x, global_y;
    wlr_scene_node_coords(&toplevel->scene_tree->node, &global_x, &global_y);

	wlr_output_layout_closest_point(toplevel->server->output_layout, output,
			global_x + geometry.x + geometry.width / 2.,
			global_y + geometry.y + geometry.height / 2.,
			&closest_x, &closest_y);

	auto *wlr_output = wlr_output_layout_output_at(toplevel->server->output_layout, closest_x, closest_y);

    if (!wlr_output) {
        return nullptr;
    }

    struct yawc_output* out;
    wl_list_for_each(out, &toplevel->server->outputs, link)
    {
        if (out->wlr_output == wlr_output) {
            return out;
        }
    }

    return nullptr;
}

bool utils::pointer_pressed(struct wlr_pointer_button_event *event){
    return event->state == wl_pointer_button_state::WL_POINTER_BUTTON_STATE_PRESSED && event->button == BTN_LEFT;
}

void utils::wake_up_from_idle(struct yawc_server *server){
    wlr_idle_notifier_v1_notify_activity(server->idle_notify_manager, server->seat);
}

struct wlr_layer_surface_v1 *utils::toplevel_layer_surface_from_surface(struct wlr_surface *surface){
    struct wlr_layer_surface_v1 *layer;
	do {
		if (!surface) {
			return nullptr;
		}

		if ((layer = wlr_layer_surface_v1_try_from_wlr_surface(surface))) {
			return layer;
		}

		if (wlr_subsurface_try_from_wlr_surface(surface)) {
			surface = wlr_surface_get_root_surface(surface);
			continue;
		}

		struct wlr_xdg_surface *xdg_surface = NULL;
		if ((xdg_surface = wlr_xdg_surface_try_from_wlr_surface(surface)) &&
				xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP && xdg_surface->popup != NULL) {
			if (!xdg_surface->popup->parent) {
				return NULL;
			}
			surface = wlr_surface_get_root_surface(xdg_surface->popup->parent);
			continue;
		}

		return nullptr;
	} while (true);
}

struct wlr_box utils::get_usable_area_of_output(struct yawc_output *output){
    auto *server = output->server;

    struct wlr_box box;
    wlr_output_layout_get_box(server->output_layout, output->wlr_output, &box);

    struct yawc_layer_surface *layer;

    wl_list_for_each(layer, &output->layer_surfaces, link) {
        struct wlr_layer_surface_v1 *surface = layer->layer_surface;

        if (!surface->surface->mapped || surface->current.exclusive_zone <= 0) {
            continue;
        }

        int zone = surface->current.exclusive_zone;
        uint32_t anchor = surface->current.anchor;

        if ((anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) && 
           !(anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
            box.y += zone;
            box.height -= zone;
        }
        else if ((anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) && 
                !(anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
            box.height -= zone;
        }
        else if ((anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) && 
                !(anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
            box.x += zone;
            box.width -= zone;
        }
        else if ((anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) && 
                !(anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
            box.width -= zone;
        }
    }

    return box;
}

void utils::exec(const char *cmd){
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", cmd, (void*)NULL);
        _exit(1);
    }
}

uint64_t utils::hash_file_fnv1a(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return 0;

    uint64_t hash = 0xcbf29ce484222325;
    char buffer[4096];

    while (file.read(buffer, sizeof(buffer)) || file.gcount()) {
        for (std::streamsize i = 0; i < file.gcount(); ++i) {
            hash ^= static_cast<uint8_t>(buffer[i]);
            hash *= 0x100000001b3;
        }
    }
    return hash;
}
