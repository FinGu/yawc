#include "server.hpp"
#include <fstream>

void on_shortcut_destroy(struct wl_listener *listener, void *data){
    struct yawc_global_shortcut *shortcut = wl_container_of(listener, shortcut, destroy);

    wl_list_remove(&shortcut->link);
    wl_list_remove(&shortcut->destroy.link);
    delete shortcut;
}

void on_new_shortcut(struct wl_listener *listener, void *data){
    struct yawc_server *server = wl_container_of(listener, server, new_shortcut);

    struct wlr_hyprland_global_shortcuts_register_shortcut_event_v1 *event = 
        reinterpret_cast<struct wlr_hyprland_global_shortcuts_register_shortcut_event_v1 *>(data);

    struct wlr_hyprland_global_shortcut_v1 *hshortcut = event->shortcut;

    struct yawc_global_shortcut *shortcut = new yawc_global_shortcut;

    shortcut->destroy.notify = on_shortcut_destroy;
    wl_signal_add(&hshortcut->events.destroy, &shortcut->destroy);

    shortcut->shortcut = hshortcut;

    wl_list_insert(&server->shortcuts, &shortcut->link);

    std::ofstream file(server->keybind_manager->global_shortcut_path.c_str(), std::ios_base::app | std::ios_base::out);  

    if(!file.is_open()){
        return;
    }

    file << hshortcut->app_id << ":" << hshortcut->id << " " << hshortcut->description << '\n';

    file.close();
}

void yawc_server::setup_shortcuts(){
    this->keybind_manager = new yawc_keybind_manager{this};

    this->shortcuts_manager = wlr_hyprland_global_shortcuts_manager_v1_create(this->wl_display, 1);    
    
    this->new_shortcut.notify = on_new_shortcut;
    wl_signal_add(&this->shortcuts_manager->events.register_shortcut, &this->new_shortcut);

    wl_list_init(&this->shortcuts);
}
