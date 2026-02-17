#include "server.hpp"

#include "shm_alloc/shm.hpp"
#include "utils.hpp"

#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>

int handle_sig_restart(int sig, void *data){
    yawc_server *server = reinterpret_cast<yawc_server*>(data);
    auto *cfg = server->config;

    cfg->load(cfg->last_path.c_str());

    yawc_pointer *pointer;
    wl_list_for_each(pointer, &server->pointers, link){
        server->load_pointer_cfg(pointer);
    }

    yawc_keyboard *keyboard;
    wl_list_for_each(keyboard, &server->keyboards, link){
        server->load_keyboard_cfg(keyboard);
    }

    if(!cfg->wm_path.empty() 
            && server->wm.hash != utils::hash_file_fnv1a(cfg->wm_path)){
        wlr_log(WLR_INFO, "Reloading the window manager: %s", cfg->wm_path.c_str());

        server->load_wm(cfg->wm_path.c_str());
    }

    if(server->keybind_manager){
        server->keybind_manager->reload();
    }

    wlr_log(WLR_INFO, "Reloaded the config");

    return 0;
}

yawc_server::yawc_server(){
    wlr_log(WLR_DEBUG, "Starting display");
    this->wl_display = wl_display_create();
    this->wl_event_loop = wl_display_get_event_loop(this->wl_display);

    wlr_log(WLR_DEBUG, "Creating backend");
    this->backend = wlr_backend_autocreate(this->wl_event_loop, &this->session);

    wlr_log(WLR_DEBUG, "Creating renderer");
    this->renderer = wlr_renderer_autocreate(this->backend);

    wlr_renderer_init_wl_shm(this->renderer, this->wl_display);

    wlr_log(WLR_DEBUG, "Creating allocator");
    this->allocator = wlr_allocator_autocreate(this->backend, this->renderer);

    wlr_log(WLR_DEBUG, "Creating shm allocator");
    this->shm_allocator = wlr_shm_allocator_create();
    
    wlr_log(WLR_DEBUG, "Creating linux dmabuf");
	if (wlr_renderer_get_texture_formats(this->renderer, WLR_BUFFER_CAP_DMABUF) != NULL) {
		this->linux_dmabuf_v1 = wlr_linux_dmabuf_v1_create_with_renderer(this->wl_display, 4, this->renderer);
	} // i think init_wl_display does that for us but i won't bother checking

	if (wlr_renderer_get_drm_fd(this->renderer) >= 0 
            && this->renderer->features.timeline 
            && this->backend->features.timeline) {
		wlr_linux_drm_syncobj_manager_v1_create(this->wl_display, 1, wlr_renderer_get_drm_fd(this->renderer));
	}

    wlr_log(WLR_DEBUG, "Creating compositor");
    this->compositor = wlr_compositor_create(this->wl_display, 5, this->renderer);

    wlr_subcompositor_create(this->wl_display);
    wlr_data_device_manager_create(this->wl_display);

    this->output_layout = wlr_output_layout_create(this->wl_display);

    this->reset_cursor_mode();

    wl_list_init(&this->outputs);
    wl_list_init(&this->toplevels);
    wl_list_init(&this->keyboards);
    wl_list_init(&this->pointers);
    wl_list_init(&this->inhibitors);

    this->reload_event = wl_event_loop_add_signal(this->wl_event_loop, SIGUSR1, handle_sig_restart, this);
    this->keybind_manager = nullptr;
}

yawc_server::~yawc_server()
{
    wlr_log(WLR_DEBUG, "Clearing server");
    
    if(this->keybind_manager){
        delete this->keybind_manager;
    }

    if(this->shortcuts_manager) {
        wl_list_remove(&this->new_shortcut.link);
    }

    if(this->wm.handle){
        this->unload_wm();
    }

    if(this->reload_event){
        wl_event_source_remove(this->reload_event);
    }

    if(this->wl_display){
        wl_display_destroy_clients(this->wl_display);
    }

    if(this->cursor_mgr){
        wlr_xcursor_manager_destroy(this->cursor_mgr);
    }

    if(this->shortcuts_manager){
        wlr_hyprland_global_shortcuts_manager_v1_destroy(this->shortcuts_manager);
    }

    if(this->cursor){
        wl_list_remove(&this->pointer_motion_listener.link);
        wl_list_remove(&this->pointer_motion_absolute_listener.link);
        wl_list_remove(&this->cursor_frame_listener.link);
        wl_list_remove(&this->cursor_button_listener.link);
        wl_list_remove(&this->on_axis_cursor_motion.link);
        wl_list_remove(&this->cursor_shape_set.link);

        wlr_cursor_destroy(this->cursor);
    }

    if (this->scene) {
        wlr_scene_node_destroy(&this->scene->tree.node);
    }

    if(this->allocator){
        wlr_allocator_destroy(this->allocator);
    }

    if(this->shm_allocator){
        wlr_allocator_destroy(this->shm_allocator);
    }

    if(this->renderer){
        wlr_renderer_destroy(this->renderer);
    }

    if(this->wl_display){
        wl_display_destroy(this->wl_display);
    }
}

void on_backend_destroy(struct wl_listener *listener, void *data){
    wlr_log(WLR_DEBUG, "Destroying backend");

    struct yawc_server* server = wl_container_of(listener, server, backend_destroy);

    wl_list_remove(&server->new_input_listener.link);
    wl_list_remove(&server->new_output_listener.link);
    wl_list_remove(&server->backend_destroy.link);
}

void yawc_server::create_scene()
{
    wlr_log(WLR_DEBUG, "Creating scene");

    this->scene = wlr_scene_create();
    this->scene_layout = wlr_scene_attach_output_layout(this->scene, this->output_layout);

    this->layers.background = wlr_scene_tree_create(&this->scene->tree);
    this->layers.bottom = wlr_scene_tree_create(&this->scene->tree);
    this->layers.normal = wlr_scene_tree_create(&this->scene->tree);
    this->layers.top = wlr_scene_tree_create(&this->scene->tree);
    this->layers.fullscreen = wlr_scene_tree_create(&this->scene->tree);
    this->layers.unmanaged = wlr_scene_tree_create(&this->scene->tree);
    this->layers.overlay = wlr_scene_tree_create(&this->scene->tree);
    this->layers.screenlock = wlr_scene_tree_create(&this->scene->tree);

    wlr_viewporter_create(this->wl_display);
	wlr_single_pixel_buffer_manager_v1_create(this->wl_display);

    this->gamma_control_manager = wlr_gamma_control_manager_v1_create(this->wl_display);
    wlr_scene_set_gamma_control_manager_v1(this->scene, this->gamma_control_manager);
}

yawc_server_error yawc_server::setup_backend(){
    wlr_log(WLR_DEBUG, "Starting backend");

    this->setup_seat();

    this->new_input_listener.notify = +[](struct wl_listener* listener, void* data) {
        struct yawc_server* server = wl_container_of(listener, server, new_input_listener);
        server->handle_new_input(listener, data);
    };
    wl_signal_add(&this->backend->events.new_input, &this->new_input_listener);

    this->new_output_listener.notify = +[](struct wl_listener* listener,
                                            void* data) {
        yawc_server* server = wl_container_of(listener, server, new_output_listener);
        server->handle_new_output(listener, data);
    };
    wl_signal_add(&this->backend->events.new_output, &this->new_output_listener);

    this->backend_destroy.notify = on_backend_destroy;
    wl_signal_add(&this->backend->events.destroy, &this->backend_destroy);

    this->setup_xwayland();
    
    if (!wlr_backend_start(this->backend)) {
        wlr_log(WLR_ERROR, "Failed to start backend");

        return yawc_server_error::FAILED_TO_START;
    }

    return yawc_server_error::OK;
}

yawc_server_error yawc_server::run() {
    wlr_log(WLR_DEBUG, "Started running");

    yawc_server_error err = handle_outputs();

    if (err) {
        wlr_log(WLR_ERROR, "Failed to start the output handler");
        return err;
    }

    create_scene();


	if (this->linux_dmabuf_v1) {
		wlr_scene_set_linux_dmabuf_v1(this->scene, this->linux_dmabuf_v1);
	}

    create_xdg_shell();

    create_idle_manager();
    create_decoration_manager();
    create_session_lock_manager();
    create_layer_shell();

    create_cursor();

    setup_screensharing();

    setup_shortcuts();

    struct wlr_xdg_foreign_registry *foreign_registry =
		wlr_xdg_foreign_registry_create(this->wl_display);
	wlr_xdg_foreign_v1_create(this->wl_display, foreign_registry);
	wlr_xdg_foreign_v2_create(this->wl_display, foreign_registry);

	wlr_presentation_create(this->wl_display, this->backend, 2);
    wlr_fractional_scale_manager_v1_create(this->wl_display, 1);
    wlr_data_control_manager_v1_create(this->wl_display);
    
    wlr_log(WLR_DEBUG, "Adding socket");

    const char* socket = wl_display_add_socket_auto(this->wl_display);

    if (!socket) {
        return yawc_server_error::FAILED_TO_CREATE_SOCKET;
    }

    setenv("WAYLAND_DISPLAY", socket, true);

    setup_backend();

    if (!this->config->autostart_cmds.empty()) {
        for(auto &cmd: this->config->autostart_cmds){
            utils::exec(cmd.c_str());
        }
    }

    if(!this->config->wm_path.empty()){
        this->load_wm(this->config->wm_path.c_str());
    }

    wl_display_run(this->wl_display);

    return yawc_server_error::OK;
}
