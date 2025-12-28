#include <linux/input-event-codes.h>
#include <set>
#include <string>
#include <cstdint>
#include <optional>
#include <vector>
#include <variant>
#include <map>

#include <toml++/toml.hpp>

struct yawc_pointer_config{
    bool enabled;

    std::optional<bool> tap_to_click;
    std::optional<bool> tap_and_drag;
    std::optional<bool> tap_drag_lock;
    
    std::optional<std::string> tap_button_map; 

    std::optional<bool> left_handed;
    std::optional<bool> nat_scrolling;
    std::optional<bool> middle_emulation;

    std::optional<std::vector<float>> calibration_matrix; 

    std::optional<std::string> scroll_method; // "2fg", "edge", "button", "none"
    std::optional<uint32_t> scroll_button;
    std::optional<bool> scroll_button_lock;
    std::optional<double> scroll_factor;

    std::optional<std::string> click_method; // "button_areas", "clickfinger"

    std::optional<bool> disable_w_typing;
    std::optional<bool> disable_w_trackpointing;

    std::optional<std::string> accel_profile; // "flat", "adaptive"
    
    std::optional<double> accel_speed;

    std::optional<float> rotation_angle;

    std::optional<std::string> map_to_output;
};

struct yawc_keyboard_config{
    bool enabled;

    std::optional<std::string> xkb_layout;
    std::optional<std::string> xkb_variant;
    std::optional<std::string> xkb_options;
    std::optional<std::string> xkb_rules;
    std::optional<std::string> xkb_model;
    
    std::optional<int32_t> repeat_rate;
    std::optional<int32_t> repeat_delay;
};

using yawc_input_config = std::variant<yawc_keyboard_config, yawc_pointer_config>;

struct yawc_bind_node{
    std::map<uint64_t, std::unique_ptr<yawc_bind_node>> children;

    std::string action; 
    bool is_global_shortcut; //if action has : in it
};

struct yawc_config{
    yawc_pointer_config default_pointer_config;
    yawc_keyboard_config default_keyboard_config;

    std::map<std::string, yawc_input_config> input_configs;
    std::vector<std::string> autostart_cmds;
    
    std::unique_ptr<struct yawc_bind_node> keybind_tree;

    bool load(std::string path);

    std::string last_path;
    std::string wm_path;

    private:
    void load_tables(toml::table &);
};


