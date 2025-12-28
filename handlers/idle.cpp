#include <tuple>

#include "../server.hpp"
#include "../utils.hpp"

void handle_inhibitor_destroy(struct wl_listener *listener, void *data){
    struct yawc_idle_inhibitor *inhibitor = wl_container_of(listener, inhibitor, on_destroy);
    auto *server = inhibitor->server;
    
    wl_list_remove(&inhibitor->on_destroy.link);
    wl_list_remove(&inhibitor->link);
    delete inhibitor;

    wlr_idle_notifier_v1_set_inhibited(server->idle_notify_manager, !wl_list_empty(&server->inhibitors));
}

void new_idle_inhibitor(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, on_new_idle_inhibitor);
    struct wlr_idle_inhibitor_v1 *inhibitor = reinterpret_cast<struct wlr_idle_inhibitor_v1*>(data);

    struct yawc_idle_inhibitor *ninhibitor = new yawc_idle_inhibitor{};
    ninhibitor->wlr_inhibitor = inhibitor;
    ninhibitor->server = server;

    ninhibitor->on_destroy.notify = handle_inhibitor_destroy;
    wl_signal_add(&inhibitor->events.destroy, &ninhibitor->on_destroy);

    wl_list_insert(&server->inhibitors, &ninhibitor->link);

    wlr_idle_notifier_v1_set_inhibited(server->idle_notify_manager, true);
}

void handle_inhibitor_manager_destroy(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, on_inhibitor_destroy);

    wlr_idle_notifier_v1_set_inhibited(server->idle_notify_manager, false);

    wl_list_remove(&server->on_new_idle_inhibitor.link);

    wl_list_remove(&server->on_inhibitor_destroy.link);

}

void yawc_server::update_idle_inhibitors(){
    if(this->cur_lock.lock){
        wlr_idle_notifier_v1_set_inhibited(this->idle_notify_manager, false);
        return;
    }

    wlr_idle_notifier_v1_set_inhibited(this->idle_notify_manager, !wl_list_empty(&this->inhibitors));
}

void yawc_server::create_idle_manager(){
    this->idle_inhibit_manager = wlr_idle_inhibit_v1_create(this->wl_display);
    this->idle_notify_manager = wlr_idle_notifier_v1_create(this->wl_display); 

    this->on_new_idle_inhibitor.notify = new_idle_inhibitor;
    wl_signal_add(&this->idle_inhibit_manager->events.new_inhibitor, &this->on_new_idle_inhibitor);

    this->on_inhibitor_destroy.notify = handle_inhibitor_manager_destroy;
    wl_signal_add(&this->idle_inhibit_manager->events.destroy, &this->on_inhibitor_destroy);
}
