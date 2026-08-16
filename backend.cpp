#include <tuple>

#include "../server.hpp"
#include "../utils.hpp"

void on_backend_destroy(struct wl_listener *listener, void *data){
    wlr_log(WLR_DEBUG, "Destroying backend");

    struct yawc_server* server = wl_container_of(listener, server, backend_destroy);

    wl_list_remove(&server->new_input_listener.link);
    wl_list_remove(&server->new_output_listener.link);
    wl_list_remove(&server->backend_destroy.link);
}

yawc_server_error yawc_server::setup_backend(){
	yawc_server_error err = yawc_server_error::OK;
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

	err = this->setup_headless_backend();
	if(err != yawc_server_error::OK){
		return err; 
	}

    this->setup_xwayland();
    
    if (!wlr_backend_start(this->backend)) {
        wlr_log(WLR_ERROR, "Failed to start backend");

        return yawc_server_error::FAILED_TO_START;
    }

    return yawc_server_error::OK;
}

yawc_output *yawc_server::create_output(struct wlr_output *wlr_output){
    struct yawc_output* output = new yawc_output;
    output->wlr_output = wlr_output;
    output->server = this;

    wlr_output->data = output;

    wl_list_insert(&this->outputs, &output->link);
    
    wl_list_init(&output->layer_surfaces);

    int width, height;
    wlr_output_effective_resolution(wlr_output, &width, &height);

    output->last_width = width;
    output->last_height = height;
    output->last_scale = wlr_output->scale;

    return output;
}

yawc_server_error yawc_server::setup_headless_backend(){
    this->headless_backend = wlr_headless_backend_create(this->wl_event_loop);

    if(!this->headless_backend){
        wlr_log(WLR_ERROR, "Couldn't create headless backend");
        return yawc_server_error::COULDNT_CREATE_HEADLESS;
    }

	wlr_multi_backend_add(this->backend, this->headless_backend);

    auto *wlr_output = wlr_headless_add_output(this->headless_backend, 800, 600);

    this->fallback_output = create_output(wlr_output);

    return yawc_server_error::OK;
}

