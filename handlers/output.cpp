#include <tuple>

#include "../lock.hpp"
#include "../toplevel.hpp"
#include "../layer.hpp"
#include "../utils.hpp"

void reorganize_toplevels(struct yawc_server *sv, struct wlr_output *old_output){
    if(wl_list_empty(&sv->outputs)){ //oh well
        return;
    }

    struct yawc_output *next_output;

    wl_list_for_each(next_output, &sv->outputs, link){
        if(next_output->wlr_output == old_output){
            continue;
        }

        if(!next_output->wlr_output->enabled){
            continue;
        } //next available output

        break;
    }

    struct wlr_box next_box;
    wlr_output_layout_get_box(sv->output_layout, next_output->wlr_output, &next_box);
    //ideally we could calculate a position relative to the old output

    struct yawc_toplevel *toplevel, *tmpt;
    wl_list_for_each_safe(toplevel, tmpt, &sv->toplevels, link){
        if(!toplevel->scene_tree){
            continue;
        }

        if(utils::get_output_of_toplevel(toplevel)->wlr_output != old_output){
            continue;
        }

        wlr_scene_node_set_position(&toplevel->scene_tree->node, next_box.x, next_box.y);
    }
}

bool apply_output_config(struct yawc_server *server,
        struct wlr_output_configuration_head_v1 *head, 
        struct wlr_output *output, 
        struct wlr_output_layout *output_layout,
        bool only_test){

    struct wlr_output_head_v1_state *state = &head->state;
    struct wlr_output_state pending;

    wlr_output_state_init(&pending);

    wlr_output_state_set_enabled(&pending, state->enabled);

    if(state->enabled){
        wlr_output_layout_add(output_layout, output, state->x, state->y);
    } else{
        wlr_output_layout_remove(output_layout, output);

        if(!only_test){
            reorganize_toplevels(server, output);
        }
    }

    if(state->mode){
        wlr_output_state_set_mode(&pending, state->mode);
    } else if(state->custom_mode.width || state->custom_mode.height){
        wlr_output_state_set_custom_mode(&pending, state->custom_mode.width, state->custom_mode.height, state->custom_mode.refresh);
    }

    if(state->scale > 0){
        wlr_output_state_set_scale(&pending, state->scale);
    }

    wlr_output_state_set_transform(&pending, state->transform);

    bool ok = wlr_output_test_state(output, &pending);

    if(only_test){
        wlr_output_state_finish(&pending);
        return ok;
    }

    ok = wlr_output_commit_state(output, &pending);
    wlr_output_state_finish(&pending);

    return ok;
}

void update_output_manager_config(struct yawc_server *server) {
    wlr_log(WLR_DEBUG, "Updating output manager config");

    struct wlr_output_configuration_v1 *cfg = wlr_output_configuration_v1_create();

    if (!cfg) {
        return;
    }

    struct yawc_output *output;
    
    if(wl_list_empty(&server->outputs)){
        wlr_output_configuration_v1_destroy(cfg);
        return;
    }

    wl_list_for_each(output, &server->outputs, link) {
        struct wlr_output_configuration_head_v1 *head =
            wlr_output_configuration_head_v1_create(cfg, output->wlr_output);

        struct wlr_box box;
        wlr_output_layout_get_box(server->output_layout, output->wlr_output, &box);

        head->state.enabled = !wlr_box_empty(&box);
        head->state.x = box.x;
        head->state.y = box.y;
    }

    wlr_output_manager_v1_set_configuration(server->output_manager, cfg);
}

void handle_output_manager_apply(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, output_manager_apply);
    struct wlr_output_configuration_v1 *config = reinterpret_cast<struct wlr_output_configuration_v1*>(data);

    struct wlr_output_configuration_head_v1 *head;

    bool ok = true;

    wl_list_for_each(head, &config->heads, link) {
        if (!(ok = apply_output_config(server, head, head->state.output, server->output_layout, true))) {
            break;
        }
    }

    if (!ok) {
        wlr_output_configuration_v1_send_failed(config);
        wlr_output_configuration_v1_destroy(config);
        return;
    }

    wl_list_for_each(head, &config->heads, link) {
        if (!(ok = apply_output_config(server, head, head->state.output, server->output_layout, false))) {
            wlr_log(WLR_ERROR, "Failed to apply config for output %s",
                    head->state.output ? head->state.output->name : "(unknown)");
        }
    }

    if (ok) {
        wlr_output_configuration_v1_send_succeeded(config);

        update_output_manager_config(server);
    } else {
        wlr_output_configuration_v1_send_failed(config);
    }

    wlr_output_configuration_v1_destroy(config);
}

void handle_output_manager_test(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, output_manager_test);
    struct wlr_output_configuration_v1 *config = reinterpret_cast<struct wlr_output_configuration_v1*>(data);

    struct wlr_output_configuration_head_v1 *head;

    wl_list_for_each(head, &config->heads, link) {
        if(!apply_output_config(server, head, head->state.output, server->output_layout, true)){
            wlr_log(WLR_ERROR, "Failed to apply config test for output %s", head->state.output->name);
            wlr_output_configuration_v1_send_failed(config);
            return;
        }
    }

    wlr_output_configuration_v1_send_succeeded(config);
}

void output_commit(struct wl_listener* listener, void* data){
    struct yawc_output *output = wl_container_of(listener, output, commit);
    struct wlr_output_event_commit *event = static_cast<struct wlr_output_event_commit*>(data);

    if (!(event->state->committed & (WLR_OUTPUT_STATE_MODE | WLR_OUTPUT_STATE_SCALE | WLR_OUTPUT_STATE_TRANSFORM))){
        return;
    }

    int width = 0, height = 0;
    wlr_output_effective_resolution(output->wlr_output, &width, &height);
    
    float scale = output->wlr_output->scale;

    bool geometry_changed = (width != output->last_width) || (height != output->last_height);
    bool scale_changed = (scale != output->last_scale);

    if (geometry_changed || scale_changed) {
        output->last_width = width;
        output->last_height = height;
        output->last_scale = scale;

        arrange_layers(output);
        arrange_locks(output->server);

        struct yawc_layer_surface *layer, *tmp;
        wl_list_for_each_safe(layer, tmp, &output->layer_surfaces, link) {
            if (layer->layer_surface && layer->layer_surface->surface) {
                wlr_fractional_scale_v1_notify_scale(layer->layer_surface->surface,
                    output->wlr_output->scale);
                wlr_surface_set_preferred_buffer_scale(layer->layer_surface->surface,
                    (int)ceilf((float)output->wlr_output->scale));
            }
        }
    }
} 

void render_frame(struct wl_listener* listener, void* data){
    struct yawc_output* output = wl_container_of(listener, output, frame);
    struct wlr_scene* scene = output->server->scene;

    struct wlr_scene_output* scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);

    struct wlr_output_state state;
    wlr_output_state_init(&state);

	if (!wlr_scene_output_build_state(scene_output, &state, NULL)) {
		wlr_output_state_finish(&state);
		return;
	}

    state.tearing_page_flip = true;

    if(!wlr_output_test_state(output->wlr_output, &state)){
        state.tearing_page_flip = false;
    }

    if (!wlr_output_commit_state(output->wlr_output, &state)) {
        wlr_log(WLR_DEBUG, "Failed to commit state");
    }
    
    wlr_output_state_finish(&state);
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
}

void request_state(struct wl_listener *listener, void *data){
    struct yawc_output* output = wl_container_of(listener, output, request_state);
    struct wlr_output_event_request_state* event = reinterpret_cast<struct wlr_output_event_request_state*>(data);

    wlr_output_commit_state(output->wlr_output, event->state);
}

void destroy_output(struct wl_listener *listener, void *data){
    wlr_log(WLR_DEBUG, "Freeing output");

    struct yawc_output* output = wl_container_of(listener, output, destroy);

    wl_list_remove(&output->commit.link);
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->request_state.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->link);

    struct yawc_layer_surface *layer, *tmpl;

    wl_list_for_each_safe(layer, tmpl, &output->layer_surfaces, link){
        layer->output = nullptr;

        wlr_layer_surface_v1_destroy(layer->layer_surface);
    }

    reorganize_toplevels(output->server, output->wlr_output);

    delete output; 
}

void yawc_server::handle_new_output(struct wl_listener* listener, void* data){
    struct wlr_output* wlr_output = reinterpret_cast<struct wlr_output*>(data);

    wlr_log(WLR_DEBUG, "Initiating render for output");
    wlr_output_init_render(wlr_output, this->allocator, this->renderer);

    struct wlr_output_state state;

    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    struct wlr_output_mode* mode = wlr_output_preferred_mode(wlr_output);
    if (mode) {
        wlr_output_state_set_mode(&state, mode);
    }

    state.tearing_page_flip = true;
    
    if(!wlr_output_test_state(wlr_output, &state)){
        state.tearing_page_flip = false;
        wlr_log(WLR_DEBUG, "tearing couldn't be applied. %s", wlr_output->name);
    }

    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);

    wlr_log(WLR_DEBUG, "Set output mode");

    struct yawc_output* output = new yawc_output;
    output->wlr_output = wlr_output;
    output->server = this;

    output->commit.notify = output_commit; 
    wl_signal_add(&wlr_output->events.commit, &output->commit);

    output->frame.notify = render_frame; 
    wl_signal_add(&wlr_output->events.frame, &output->frame);

    output->request_state.notify = request_state; 
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);

    output->destroy.notify = destroy_output;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    wlr_output->data = output;

    wl_list_insert(&this->outputs, &output->link);
    
    wl_list_init(&output->layer_surfaces);

    struct wlr_output_layout_output* l_output = wlr_output_layout_add_auto(this->output_layout, wlr_output);
    struct wlr_scene_output* scene_output = wlr_scene_output_create(this->scene, wlr_output);

    update_output_manager_config(this);

    int width, height;
    wlr_output_effective_resolution(wlr_output, &width, &height);

    output->last_width = width;
    output->last_height = height;
    output->last_scale = wlr_output->scale;

    if(this->cur_lock.lock){
        session_lock_output_create(this, output);
    }

    wlr_scene_output_layout_add_output(this->scene_layout, l_output,
        scene_output);
}

void on_output_manager_destroy(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, output_manager_destroy); 

    wl_list_remove(&server->output_manager_apply.link);
    wl_list_remove(&server->output_manager_test.link);
    wl_list_remove(&server->output_manager_destroy.link);
}

yawc_server_error yawc_server::handle_outputs()
{
    wlr_log(WLR_DEBUG, "Handling outputs");
    
    this->output_manager = wlr_output_manager_v1_create(this->wl_display);
    
    if(!this->output_manager){
        wlr_log(WLR_DEBUG, "Output manager is not available");
        return yawc_server_error::FAILED_TO_START;
    }

    this->xdg_output_manager = wlr_xdg_output_manager_v1_create(this->wl_display, this->output_layout);

    if(!this->xdg_output_manager){
        wlr_log(WLR_DEBUG, "XDG output manager is not available");
        return yawc_server_error::FAILED_TO_START;
    }

    this->output_manager_apply.notify = handle_output_manager_apply;
    wl_signal_add(&this->output_manager->events.apply, &this->output_manager_apply);

    this->output_manager_test.notify = handle_output_manager_test;
    wl_signal_add(&this->output_manager->events.test, &this->output_manager_test);

    this->output_manager_destroy.notify = on_output_manager_destroy;
    wl_signal_add(&this->output_manager->events.destroy, &this->output_manager_destroy);

    update_output_manager_config(this);

    return yawc_server_error::OK;
}

