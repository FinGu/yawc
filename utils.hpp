#include <tuple>
#include <cstdint>
#include <string>

struct yawc_server;
struct yawc_toplevel;
struct yawc_toplevel_decoration;
struct yawc_toplevel_geometry;
struct yawc_input_on_surface;
struct yawc_input_on_node;
struct yawc_output;
struct wlr_pointer_button_event;
struct wlr_scene_node;
struct wlr_layer_surface_v1;
struct wlr_surface;
struct wlr_box;

namespace utils {
    std::tuple<yawc_toplevel*, yawc_input_on_surface>
        desktop_toplevel_at(yawc_server* server, double lx, double ly);

    std::tuple<wlr_scene_node*, yawc_input_on_node>
        desktop_node_at(yawc_server* server, double lx, double ly);

    struct yawc_toplevel *previous_toplevel(struct yawc_server *server);

    void focus_toplevel(struct yawc_toplevel* toplevel);

    void unfocus_everything(); // should hardly be used

    bool toplevel_not_empty(struct yawc_toplevel* toplevel);

    struct wlr_box get_geometry_of_toplevel(struct yawc_toplevel* toplevel);

    struct yawc_output* get_output_of_toplevel(struct yawc_toplevel* toplevel);

    void update_geometry_of_toplevel(struct yawc_toplevel *toplevel, struct wlr_box *box);

    bool pointer_pressed(struct wlr_pointer_button_event *event);

    void wake_up_from_idle(struct yawc_server *server);

    void exec(const char *cmd);

    struct wlr_layer_surface_v1 *toplevel_layer_surface_from_surface(struct wlr_surface *surface);
    struct wlr_box get_usable_area_of_output(struct yawc_output *output);

    uint64_t hash_file_fnv1a(const std::string& path);
} // namespace utils
