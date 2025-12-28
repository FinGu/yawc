/*#include "wm_defs.hpp"

#include "decoration.hpp"

#include "utils.hpp"

#include "window_ops.hpp"

static uint64_t current_id = 0;

void handle_toplevel_foreign_activate(struct wl_listener* listener, void* data){
    wlr_log(WLR_DEBUG, "Foreign activate requested");

    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, foreign_request_activate);

    if(!toplevel->mapped){
        return;
    }

    toplevel->request_activate();
}

void handle_toplevel_foreign_close(struct wl_listener* listener, void* data){
    wlr_log(WLR_DEBUG, "Foreign close requested");

    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, foreign_request_close);

    if(!toplevel->mapped){
        return;
    }

    toplevel->request_close();
}

void handle_toplevel_foreign_minimize(struct wl_listener* listener, void* data){
    wlr_log(WLR_DEBUG, "Foreign minimize requested");

    struct yawc_toplevel* toplevel = wl_container_of(listener, toplevel, foreign_request_minimize);

    if(!toplevel->mapped){
        return;
    }

    toplevel->request_minimize(true);
}

void yawc_toplevel::send_geometry_update(bool reverse){
    struct wlr_box geo = utils::get_geometry_of_toplevel(this);

    auto *last_geo = &this->last_geo;

    if(last_geo->height != geo.height 
            || last_geo->width != geo.width){

        if(server->callbacks && server->callbacks->on_geometry){
            wm_toplevel wm_tl {this};

            wm_box_t last_box {last_geo->x, last_geo->y, last_geo->width, last_geo->height};
            wm_box_t cur_box {geo.x, geo.y, geo.width, geo.height};

            //in case we're resetting the state, we have to send the right event
            if(reverse){
                server->callbacks->on_geometry(&wm_tl, cur_box, last_box);
            } else{
                server->callbacks->on_geometry(&wm_tl, last_box, cur_box);
            }
        }

        if (this->type == YAWC_TOPLEVEL_XWAYLAND && this->xwayland_surface) {
            wlr_xwayland_surface_configure(
                this->xwayland_surface,
                geo.x, geo.y,
                geo.width, geo.height
            );
        }

        if(!this->maximized){
            this->save_state();
        }
    }
}

void yawc_toplevel::activate(bool yes){
    if(this->type == YAWC_TOPLEVEL_XWAYLAND){
        wlr_xwayland_surface_activate(this->xwayland_surface, yes);
    } else{
        wlr_xdg_toplevel_set_activated(this->xdg_toplevel, yes);
    }

    if(this->foreign_handle){
        wlr_foreign_toplevel_handle_v1_set_activated(this->foreign_handle, yes);
    }
}

void yawc_toplevel::map(){
    auto *output = utils::get_output_of_toplevel(this);

    if(!output){
        return;
    }

    int output_width, output_height;
    wlr_output_effective_resolution(output->wlr_output, &output_width, &output_height);

    float scale = output->wlr_output->scale;

    struct wlr_surface *surface = this->type == YAWC_TOPLEVEL_XDG 
        ? this->xdg_toplevel->base->surface : this->xwayland_surface->surface; 

	wlr_fractional_scale_v1_notify_scale(surface, scale);
	wlr_surface_set_preferred_buffer_scale(surface, ceil(scale));
    
    if(this->decoration){
        setup_decoration(this->decoration);
    }

    this->foreign_handle = 
        wlr_foreign_toplevel_handle_v1_create(this->server->foreign_toplevel_manager); 

    this->foreign_request_minimize.notify = handle_toplevel_foreign_minimize; 
    wl_signal_add(&this->foreign_handle->events.request_minimize, &this->foreign_request_minimize);

    this->foreign_request_activate.notify = handle_toplevel_foreign_activate; 
    wl_signal_add(&this->foreign_handle->events.request_activate, &this->foreign_request_activate);

    this->foreign_request_close.notify = handle_toplevel_foreign_close; 
    wl_signal_add(&this->foreign_handle->events.request_close, &this->foreign_request_close);

    switch(this->type){
        case YAWC_TOPLEVEL_XWAYLAND:
            if(!this->xwayland_surface){
                break;
            }

            this->set_title(this->xwayland_surface->title);
            this->set_app_id(this->xwayland_surface->class_);

            this->image_capture_scene_surface = wlr_scene_surface_create(&this->image_capture_scene->tree, 
                    this->xwayland_surface->surface);

            break;
        case YAWC_TOPLEVEL_XDG:
            this->set_title(this->xdg_toplevel->title);
            this->set_app_id(this->xdg_toplevel->app_id);

            this->image_capture_scene_surface = wlr_scene_surface_create(&this->image_capture_scene->tree, 
                    this->xdg_toplevel->base->surface);

            break;

        default:
            break;
    }

    this->mapped = true;

    if(server->callbacks && server->callbacks->on_map){
        wm_toplevel wm_tl {this};

        server->callbacks->on_map(&wm_tl);

        return;
    }   
    
    if(type == YAWC_TOPLEVEL_XDG){
        auto *requested = &this->xdg_toplevel->requested;
        
        if(requested->fullscreen){
            this->set_fullscreen(true);
        } else if(requested->maximized){
            this->default_set_maximized(true);
        }
    } else{
        if(xwayland_surface->fullscreen){
            this->set_fullscreen(true);
        } else if(xwayland_surface->maximized_horz && xwayland_surface->maximized_vert){
            this->default_set_maximized(true);
        }
    }
}

void yawc_toplevel::unmap(){
    if (this == this->server->grabbed_toplevel) {
        this->server->reset_cursor_mode();
    }

    if(server->callbacks && server->callbacks->on_unmap){
        wm_toplevel wm_tl {this};

        server->callbacks->on_unmap(&wm_tl);
    }

    if(this->foreign_handle){
        wl_list_remove(&this->foreign_request_activate.link);
        wl_list_remove(&this->foreign_request_close.link);
        wl_list_remove(&this->foreign_request_minimize.link);

        wlr_foreign_toplevel_handle_v1_destroy(this->foreign_handle);
    }

    if(this->image_capture_scene_surface){
	    wlr_scene_node_destroy(&this->image_capture_scene_surface->buffer->node);
	    this->image_capture_scene_surface = nullptr;
    }

    this->mapped = false;
}

void yawc_toplevel::commit(){
    const char *current_title = nullptr;
    const char *current_app_id = nullptr;
    struct wlr_box current_geo = {0};

    if (this->type == YAWC_TOPLEVEL_XDG) {
        current_title = this->xdg_toplevel->title;
        current_app_id = this->xdg_toplevel->app_id;
        current_geo = this->xdg_toplevel->base->geometry;
    } else {
        current_title = this->xwayland_surface->title ? this->xwayland_surface->title : nullptr;
        current_app_id = this->xwayland_surface->class_ ? this->xwayland_surface->class_ : nullptr;
        
        current_geo.x = this->xwayland_surface->x;
        current_geo.y = this->xwayland_surface->y;
        current_geo.width = this->xwayland_surface->width;
        current_geo.height = this->xwayland_surface->height;
    }

    if(server->callbacks && server->callbacks->on_commit){
        wm_toplevel wm_tl {this}; 

        server->callbacks->on_commit(&wm_tl);
    }

    if(!this->mapped){
        return;
    }

    this->set_title(current_title);

    this->set_app_id(current_app_id);

    this->send_geometry_update();
}

void yawc_toplevel::request_maximize(bool enable){
    utils::wake_up_from_idle(this->server);

    if(server->callbacks && server->callbacks->on_toplevel_request_event){
        wm_toplevel ctoplevel {this};

        wm_toggle_request_payload payload {enable};

        auto cevent = 
            wm_create_toplevel_request_event(&ctoplevel, wm_toplevel_request_type_t::WM_REQUEST_MAXIMIZE, &payload);

        server->callbacks->on_toplevel_request_event(&cevent);

        return;
    }

    this->default_set_maximized(enable);
}

void yawc_toplevel::request_minimize(bool enable){
    utils::wake_up_from_idle(this->server);

    if(server->callbacks && server->callbacks->on_toplevel_request_event){
        wm_toplevel ctoplevel {this};

        wm_toggle_request_payload payload{enable};

        auto cevent = 
            wm_create_toplevel_request_event(&ctoplevel, wm_toplevel_request_type_t::WM_REQUEST_MINIMIZE, &payload);

        server->callbacks->on_toplevel_request_event(&cevent);

        return;
    }

    this->default_set_minimized(enable);
}

void yawc_toplevel::request_fullscreen(bool enable){
    utils::wake_up_from_idle(this->server);

    if (server->callbacks && server->callbacks->on_toplevel_request_event) {
        wm_toplevel wm_tl {this};
        wm_toggle_request_payload payload {enable};

        auto cevent = 
            wm_create_toplevel_request_event(&wm_tl, WM_REQUEST_FULLSCREEN, &payload);
        
        server->callbacks->on_toplevel_request_event(&cevent);
        return; 
    }

    if (this->fullscreen == enable) {
        return;
    }

    this->set_fullscreen(enable);
}

void yawc_toplevel::request_move(){
    utils::wake_up_from_idle(this->server);

    if(server->callbacks && server->callbacks->on_toplevel_request_event){
        wm_toplevel ctoplevel {this};

        auto cevent = 
            wm_create_toplevel_request_event(&ctoplevel, wm_toplevel_request_type_t::WM_REQUEST_MOVE, NULL);

        server->callbacks->on_toplevel_request_event(&cevent);

        return;
    }

    if(this->fullscreen){
        return;
    }

    if(this->maximized){
        this->default_set_maximized(false);
    }
    
    begin_move(this);
}

void yawc_toplevel::request_resize(uint32_t edges){
    utils::wake_up_from_idle(this->server);

    auto *server = this->server;

    if(server->callbacks && server->callbacks->on_toplevel_request_event){
        wm_toplevel ctoplevel {this};
        
        wm_resize_request_payload payload{edges};

        auto cevent = 
            wm_create_toplevel_request_event(&ctoplevel, wm_toplevel_request_type_t::WM_REQUEST_RESIZE, &payload);

        server->callbacks->on_toplevel_request_event(&cevent);

        return;
    }

    begin_resize(this, edges);
}

void yawc_toplevel::request_activate(){
    auto *server = this->server;

    utils::wake_up_from_idle(server);

    if(server->callbacks && server->callbacks->on_toplevel_request_event){
        wm_toplevel ctoplevel {this};

        auto cevent = 
            wm_create_toplevel_request_event(&ctoplevel, wm_toplevel_request_type_t::WM_REQUEST_ACTIVATE, NULL);

        server->callbacks->on_toplevel_request_event(&cevent);

        return;
    }

    this->default_set_minimized(false);

    utils::focus_toplevel(this);
}

void yawc_toplevel::request_close(){
    auto *server = this->server;

    utils::wake_up_from_idle(server);

    if(server->callbacks && server->callbacks->on_toplevel_request_event){
        wm_toplevel ctoplevel {this};

        auto cevent = 
            wm_create_toplevel_request_event(&ctoplevel, wm_toplevel_request_type_t::WM_REQUEST_CLOSE, NULL);

        server->callbacks->on_toplevel_request_event(&cevent);

        return;
    }

    if(type == YAWC_TOPLEVEL_XDG){
        wlr_xdg_toplevel_send_close(this->xdg_toplevel);
    } else{
        wlr_xwayland_surface_close(this->xwayland_surface);
    }
}

struct yawc_toplevel_geometry yawc_toplevel::reset_state(){
    auto last_geo = this->last_geo;

    wlr_scene_node_set_position(&this->scene_tree->node, last_geo.x,
        last_geo.y);

    if (this->type == YAWC_TOPLEVEL_XDG) {
        wlr_xdg_toplevel_set_size(this->xdg_toplevel, 
            last_geo.width, last_geo.height);
    }  else {
        this->send_geometry_update(true); //xwayland async bs

        wlr_xwayland_surface_configure(this->xwayland_surface,
            last_geo.x, last_geo.y, 
            last_geo.width, last_geo.height);
    }

    return last_geo;
}

struct yawc_toplevel_geometry yawc_toplevel::save_state(){
    int current_x, current_y;
    wlr_scene_node_coords(&this->scene_tree->node, &current_x, &current_y);

    auto geometry = utils::get_geometry_of_toplevel(this);
   
    struct yawc_toplevel_geometry geo = {
        current_x, current_y, geometry.width, geometry.height
    };

    this->last_geo = geo;

    return geo;
}

void yawc_toplevel::set_title(const char *title){
    if(!title){
        return;
    }

    if(this->title == title){
        return;
    }

    if(this->foreign_handle){
        wlr_foreign_toplevel_handle_v1_set_title(this->foreign_handle, title);
    }

    this->title = title;
}

void yawc_toplevel::set_app_id(const char *app_id){
    if(!app_id){
        return;
    }

    if(this->app_id == app_id){
        return;
    }

    if(this->foreign_handle){
        wlr_foreign_toplevel_handle_v1_set_app_id(this->foreign_handle, app_id);
    }

    this->app_id = app_id;
}

void on_toplevel_output_handler_destroy(struct wl_listener *listener, void *data){
    struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, foreign_output_handler_destroy);

    wl_list_remove(&toplevel->foreign_output_enter.link);
    wl_list_remove(&toplevel->foreign_output_leave.link);
    wl_list_remove(&toplevel->foreign_output_handler_destroy.link);
}

void on_toplevel_output_enter(struct wl_listener *listener, void *data){
    struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, foreign_output_enter);

    if(!toplevel->foreign_handle){
        return;
    }

	struct wlr_scene_output *output = reinterpret_cast<struct wlr_scene_output *>(data);

    wlr_foreign_toplevel_handle_v1_output_enter(toplevel->foreign_handle, output->output);
}

void on_toplevel_output_leave(struct wl_listener *listener, void *data){
    struct yawc_toplevel *toplevel = wl_container_of(listener, toplevel, foreign_output_leave);

    if(!toplevel->foreign_handle){
        return;
    }

	struct wlr_scene_output *output = reinterpret_cast<struct wlr_scene_output *>(data);

    wlr_foreign_toplevel_handle_v1_output_leave(toplevel->foreign_handle, output->output);
}

void yawc_toplevel::setup_foreign_output_handler(){
    this->foreign_on_output_handler = wlr_scene_buffer_create(this->scene_tree, NULL);

    this->foreign_output_enter.notify = on_toplevel_output_enter;
    wl_signal_add(&this->foreign_on_output_handler->events.output_enter, &this->foreign_output_enter);

    this->foreign_output_leave.notify = on_toplevel_output_leave;
    wl_signal_add(&this->foreign_on_output_handler->events.output_leave, &this->foreign_output_leave);

    this->foreign_output_handler_destroy.notify = on_toplevel_output_handler_destroy;
    wl_signal_add(&this->foreign_on_output_handler->node.events.destroy, &this->foreign_output_handler_destroy);
}

yawc_toplevel *create_toplevel_xdg(yawc_server *server, struct wlr_xdg_toplevel *xdg_toplevel){
    auto *toplevel = new yawc_toplevel{};

    toplevel->hidden = false;
    toplevel->id = current_id++;

    toplevel->server = server;

    toplevel->xdg_toplevel = xdg_toplevel;
    toplevel->type = YAWC_TOPLEVEL_XDG;
    
	toplevel->image_capture_scene = wlr_scene_create();

    wl_list_insert(&toplevel->server->toplevels, &toplevel->link);

    toplevel->scene_tree = wlr_scene_xdg_surface_create(server->layers.normal, xdg_toplevel->base);

    if (!scene_descriptor_assign(&toplevel->scene_tree->node,
            YAWC_SCENE_DESC_VIEW, toplevel, 0)) {
        wlr_log(WLR_ERROR, "Failed to assign scene descriptor to toplevel");
        wlr_scene_node_destroy(&toplevel->scene_tree->node);
        delete toplevel;
        return nullptr;
    }

    xdg_toplevel->base->data = toplevel->scene_tree;

    toplevel->has_resize_grips = false;
    toplevel->wm_state = nullptr;
    toplevel->last_geo = {0};

    toplevel->setup_foreign_output_handler();

    return toplevel;
}

yawc_toplevel *create_toplevel_xwl(yawc_server *server, struct wlr_xwayland_surface *xsurface){
    auto *toplevel = new yawc_toplevel{};

    toplevel->hidden = false;
    toplevel->id = current_id++;

    toplevel->server = server;

    toplevel->xwayland_surface = xsurface;
    toplevel->type = YAWC_TOPLEVEL_XWAYLAND;

    //the real surface is only valid when mapping xsurface->surface
    toplevel->has_resize_grips = false;
    toplevel->wm_state = nullptr;
    toplevel->last_geo = {0};

    toplevel->image_capture_scene = wlr_scene_create();
    wl_list_insert(&toplevel->server->toplevels, &toplevel->link);

    return toplevel;
}

yawc_toplevel_unmanaged *create_toplevel_xwl_unmanaged(yawc_server *server, struct wlr_xwayland_surface *wayland_surface){
    auto *unmanaged = new yawc_toplevel_unmanaged{};
    unmanaged->server = server;
    unmanaged->xsurface = wayland_surface;

    return unmanaged;
}

yawc_toplevel::~yawc_toplevel(){
    wl_list_remove(&this->link);
    wlr_scene_node_destroy(&this->image_capture_scene->tree.node);
}*/
