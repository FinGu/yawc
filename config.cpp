#include "config.hpp"

#include <vector>
#include <array>
#include <algorithm>
#include <wlr/util/log.h>
#include <wlr/types/wlr_keyboard.h>
#include <cstdlib>
#include <ranges>
#include <string>

yawc_keyboard_config parse_keyboard_config(toml::table &table){
    yawc_keyboard_config out;

    out.enabled = table["enabled"].value_or(true);

    out.xkb_layout = table["xkb_layout"].value<std::string>();
    out.xkb_variant = table["xkb_variant"].value<std::string>();
    out.xkb_options = table["xkb_options"].value<std::string>();
    out.xkb_rules = table["xkb_rules"].value<std::string>();
    out.xkb_model = table["xkb_model"].value<std::string>();

    out.repeat_rate = table["repeat_rate"].value<int32_t>();
    out.repeat_delay = table["repeat_delay"].value<int32_t>();

    return out;
}

yawc_pointer_config parse_pointer_config(toml::table &table){
    yawc_pointer_config out;

    out.enabled = table["enabled"].value_or(true);

    out.tap_to_click = table["tap_to_click"].value<bool>();
    out.tap_and_drag = table["tap_and_drag"].value<bool>();
    out.tap_drag_lock = table["tap_drag_lock"].value<bool>();

    out.tap_button_map = table["tap_button_map"].value<std::string>();

    out.left_handed = table["left_handed"].value<bool>();
    out.nat_scrolling = table["nat_scrolling"].value<bool>();
    out.middle_emulation = table["middle_emulation"].value<bool>();

    if(toml::array *arr = table["calibration_matrix"].as_array()){
        std::vector<float> calm;         

        arr->for_each([&](toml::value<double> &el) mutable{
            calm.push_back(*el);
        });

        out.calibration_matrix = calm;
    }

    out.scroll_method = table["scroll_method"].value<std::string>();
    out.scroll_button = table["scroll_button"].value<uint32_t>();
    out.scroll_button_lock = table["scroll_button_lock"].value<bool>();
    out.scroll_factor = table["scroll_factor"].value<double>();

    out.click_method = table["click_method"].value<std::string>();

    out.accel_profile = table["accel_profile"].value<std::string>();
    out.accel_speed = table["accel_speed"].value<double>();

    out.disable_w_typing = table["disable_w_typing"].value<bool>();
    out.disable_w_trackpointing = table["disable_w_trackpointing"].value<bool>();

    out.accel_profile = table["accel_profile"].value<std::string>();
    out.accel_speed = table["accel_speed"].value<double>();

    out.rotation_angle = table["rotation_angle"].value<float>();

    out.map_to_output = table["map_to_output"].value<std::string>();

    return out;
}

constexpr std::array<std::string_view, 5> reserved_tables = {
    "pointer",
    "keyboard",
    "environment",
    "keybinds",
};

yawc_input_config attempt_keyboard_and_pointer(toml::table &table){
    if (table.contains("xkb_layout") || table.contains("xkb_options") || table.contains("repeat_rate")) {
        return parse_keyboard_config(table);
    }
    
    return parse_pointer_config(table);
}

void parse_keybind(struct yawc_bind_node *bind, std::string_view key, std::string_view value){
    bool is_global_shortcut = value.contains(':');

    auto words = key | std::views::split(',');

    struct yawc_bind_node *cur_node = bind;

    for(auto el : words){
        auto key_parts = std::string_view{el} | std::views::split('+');

        uint32_t button, modifiers; 
        button = modifiers = 0;

        for(auto single: key_parts){
            auto view = std::string{std::string_view{single}};

            if(view == "Shift"){
                modifiers |= WLR_MODIFIER_SHIFT;
            } else if(view == "Control" || view == "Ctrl"){
                modifiers |= WLR_MODIFIER_CTRL;
            } else if(view == "Alt"){
                modifiers |= WLR_MODIFIER_ALT;
            } else if(view == "Super" || view == "Win"){
                modifiers |= WLR_MODIFIER_LOGO;
            } else{
                button = xkb_keysym_from_name(view.c_str(), XKB_KEYSYM_NO_FLAGS);
            }
        }
        
        uint64_t id = ((uint64_t)modifiers << 32) + button;

        auto *children = &cur_node->children;

        if(children->find(id) != children->end()){
            cur_node = children->at(id).get();
        } else{
            auto new_node = std::make_unique<struct yawc_bind_node>();

            cur_node = children->emplace(id, std::move(new_node)).first->second.get();
        }
    }

    cur_node->action = value; 
    cur_node->is_global_shortcut = is_global_shortcut;
}

void yawc_config::load_tables(toml::table &table){
    auto window_manager = table["window_manager"];
    
    if(toml::value<std::string> *wm = window_manager.as_string()){
        this->wm_path = wm->get();
    }

    auto pointer = table["pointer"];

    if(toml::table *pointer_table = pointer.as_table()){
        this->default_pointer_config = parse_pointer_config(*pointer_table);
    }

    auto keyboard = table["keyboard"];

    if(toml::table *keyboard_table = keyboard.as_table()){
        this->default_keyboard_config = parse_keyboard_config(*keyboard_table);
    }

    auto environment = table["environment"];

    if(toml::table *env_table = environment.as_table()){
        for(auto &&[key, inner]: *env_table){
            std::string buf{key.str()};

            setenv(key.str().data(), inner.value_or(""), true);
        }
    }

    auto autostart = table["autostart"];

    if (toml::array *arr = autostart.as_array()) {
        arr->for_each([&](toml::value<std::string> &el) {
            this->autostart_cmds.push_back(*el);
        });
    }

    this->keybind_tree = std::make_unique<struct yawc_bind_node>();

    if(toml::table *keybinds = table["keybinds"].as_table()){
        for(auto &&[key, inner]: *keybinds){
            toml::value<std::string> *as_str = inner.as_string();

            if(!as_str){
                continue;
            }

            std::string processed_key = std::string{key.str().data()};

            processed_key.erase(std::remove(processed_key.begin(), processed_key.end(), ' '), processed_key.end());
            
            parse_keybind(this->keybind_tree.get(), processed_key, **as_str);
        }
    }    

    for(auto &&[key, inner]: table){
        toml::table *as_table = inner.as_table();

        if(!as_table){
            continue;
        }

        toml::table table = *as_table;

        if(std::find(reserved_tables.begin(), reserved_tables.end(), key) != reserved_tables.end()){
            continue; 
        }

        const char *name = key.str().data(); 

        if(table["type"] == "keyboard"){
            this->input_configs[name] = parse_keyboard_config(table);
        } else if(table["type"] == "pointer"){
            this->input_configs[name] = parse_pointer_config(table);
        } else{
            this->input_configs[name] = attempt_keyboard_and_pointer(table);
        }
    }
}

bool yawc_config::load(std::string path){
    default_pointer_config = {.enabled = true};
    default_keyboard_config = {.enabled = true};
    input_configs.clear();
    autostart_cmds.clear();

    this->last_path = path;

    try{
        toml::table result = toml::parse_file(path);

        load_tables(result);

        return true;
    } catch(...){
        puts("Failed to load the config");
        return false;
    }
}
