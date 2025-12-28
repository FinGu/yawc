#ifndef WLR_TYPES_HYPRLAND_GLOBAL_SHORTCUTS_V1_H
#define WLR_TYPES_HYPRLAND_GLOBAL_SHORTCUTS_V1_H 

#include <wayland-server-core.h>

struct wlr_hyprland_global_shortcuts_manager_v1;
struct wlr_hyprland_global_shortcut_v1;

struct wlr_hyprland_global_shortcuts_manager_v1{
	struct wl_global *global;
	struct wl_list resources;

	struct {
		struct wl_signal register_shortcut;
		struct wl_signal destroy; 
	} events;

	void *data;
};

struct wlr_hyprland_global_shortcuts_register_shortcut_event_v1{
    struct wlr_hyprland_global_shortcut_v1 *shortcut; 
};

struct wlr_hyprland_global_shortcut_v1 {
	struct wl_resource *resource;
	struct wlr_hyprland_global_shortcuts_manager_v1 *manager;
	struct wl_list link;

	char *id;
    char *app_id;
    char *description;
    char *trigger_description;

	struct{
		struct wl_signal destroy;	
	} events;

	void *data;
};

void wlr_hyprland_global_shortcut_v1_send_pressed(
    struct wlr_hyprland_global_shortcut_v1 *shortcut, 
    uint32_t time_hi, uint32_t time_lo, uint32_t time_ns);

void wlr_hyprland_global_shortcut_v1_send_released(
    struct wlr_hyprland_global_shortcut_v1 *shortcut, 
    uint32_t time_hi, uint32_t time_lo, uint32_t time_ns);

struct wlr_hyprland_global_shortcuts_manager_v1 *wlr_hyprland_global_shortcuts_manager_v1_create(struct wl_display *display, uint32_t version);

void wlr_hyprland_global_shortcuts_manager_v1_destroy(struct wlr_hyprland_global_shortcuts_manager_v1 *manager);

#endif
