#include <ctime>
#include <ranges>
#include <string>

#include "server.hpp"
#include "utils.hpp"

yawc_keybind_manager::yawc_keybind_manager(struct yawc_server *server){
    this->server = server;

    this->global_shortcut_path = "/tmp/yawc_global_shortcuts.log";
    remove(this->global_shortcut_path.c_str());

    this->reload();
}

void yawc_keybind_manager::reload(){
    this->cur_node = this->server->config->keybind_tree.get();

    this->cur_global_shortcut = nullptr;
    this->cur_pressed_button = 0; 
    this->last_time = time(nullptr);
}

bool yawc_keybind_manager::in_sequence(){
    return this->cur_node != this->server->config->keybind_tree.get();
}

void yawc_keybind_manager::store(uint32_t button, uint32_t modifiers, bool pressed){
    if(!pressed){
        this->pressed_keys.erase(button);

        if(this->cur_global_shortcut && this->cur_pressed_button == button){
            wlr_hyprland_global_shortcut_v1_send_released(this->cur_global_shortcut->shortcut, 0, 0, 0);
            this->cur_global_shortcut = nullptr;
            this->cur_pressed_button = 0;
        }

        return;
    }

    this->pressed_keys.insert(button);

    uint64_t id = ((uint64_t)modifiers << 32) + button;

    time_t now = time(nullptr);
    
    if(now - this->last_time > 1 && this->in_sequence()){ // > 1 second timeout
        this->cur_node = this->server->config->keybind_tree.get();
    }

    if(this->cur_node->children.contains(id)){
        this->cur_node = this->cur_node->children.at(id).get();
        this->last_time = time(nullptr);
        return;
    } 

    this->cur_node = this->server->config->keybind_tree.get(); //reset and recheck

    if(this->cur_node->children.contains(id)){
        this->cur_node = this->cur_node->children.at(id).get();
        this->last_time = time(nullptr);
    }
}

struct yawc_bind_node *yawc_keybind_manager::triggered(){
    time_t now = time(nullptr);

    if(now - this->last_time > 1 && this->in_sequence()){
        this->cur_node = this->server->config->keybind_tree.get();
        return nullptr;
    }

    auto *node = this->cur_node;
    
    if(node->action.empty() || !node->children.empty()){
        return nullptr;
    }

    this->cur_node = this->server->config->keybind_tree.get();

    return node;
}

bool yawc_keybind_manager::needed(){
    return this->cur_node;
}

void yawc_keybind_manager::execute_action(struct yawc_bind_node *node, uint32_t trigger){
    auto *server = this->server;

    if(!node->is_global_shortcut){
        wlr_log(WLR_DEBUG, "Executing command: %s", node->action.c_str());

        utils::exec(node->action.c_str());

        return; 
    }
    
    auto parts_view = node->action | std::views::split(':');
    auto it = parts_view.begin();
    auto end = parts_view.end();

    if (it == end) {
        return;
    }

    std::string app_id((*it).begin(), (*it).end());
        
    if (++it == end) {
        return;
    }

    std::string id((*it).begin(), (*it).end());

    struct yawc_global_shortcut *gshorcut;
    wl_list_for_each(gshorcut, &server->shortcuts, link){
        if(app_id == gshorcut->shortcut->app_id && id == gshorcut->shortcut->id){
            this->cur_global_shortcut = gshorcut;
            this->cur_pressed_button = trigger;

            wlr_hyprland_global_shortcut_v1_send_pressed(gshorcut->shortcut, 0, 0, 0);
            
            wlr_log(WLR_DEBUG, "Triggered global shortcut: %s %s", app_id.c_str(), id.c_str());
        }
    }
}
