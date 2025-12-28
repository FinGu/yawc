#include "hyprland-global-shortcuts-v1-protocol.h"
#include "hyprland-global-shortcuts-v1.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define HYPRLAND_GLOBAL_SHORTCUTS_V1_VERSION 1

static const struct hyprland_global_shortcuts_manager_v1_interface shortcuts_manager_impl;
static const struct hyprland_global_shortcut_v1_interface shortcut_impl;

static void hypr_global_shortcuts_handle_resource_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void hypr_shortcut_handle_resource_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void hypr_shortcut_handle_destroy(struct wl_resource *resource) {
	struct wlr_hyprland_global_shortcut_v1 *shortcut = wl_resource_get_user_data(resource);

	wl_signal_emit(&shortcut->events.destroy, shortcut);

	wl_list_remove(&shortcut->link);
	wl_resource_set_user_data(shortcut->resource, NULL);

	free(shortcut->id);
	free(shortcut->app_id);
	free(shortcut->description);
	free(shortcut->trigger_description);

	assert(wl_list_empty(&shortcut->events.destroy.listener_list));

	free(shortcut);
}

static void hypr_global_shortcuts_handle_register_shortcut(
struct wl_client *client,
struct wl_resource *resource,
uint32_t new_id,
const char *id,
const char *app_id,
const char *description,
const char *trigger_description){
	struct wlr_hyprland_global_shortcuts_manager_v1 *manager 
		= wl_resource_get_user_data(resource);

	struct wlr_hyprland_global_shortcut_v1 *existing;
    wl_list_for_each(existing, &manager->resources, link) {
        if (strcmp(existing->app_id, app_id) == 0 && 
            strcmp(existing->id, id) == 0) {
            
            wl_resource_post_error(resource,
                HYPRLAND_GLOBAL_SHORTCUTS_MANAGER_V1_ERROR_ALREADY_TAKEN,
                "Shortcut already registered");
            return;
        }
    }

	uint32_t version = wl_resource_get_version(resource);
	struct wl_resource *shortcut_resource = wl_resource_create(client,
		&hyprland_global_shortcut_v1_interface, version, new_id);

	if (shortcut_resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(shortcut_resource, &shortcut_impl, NULL, hypr_shortcut_handle_destroy);

	struct wlr_hyprland_global_shortcut_v1 *global_shortcut = calloc(1, sizeof(*global_shortcut));
	if (global_shortcut == NULL) {
		wl_resource_destroy(shortcut_resource); 
        wl_client_post_no_memory(client);
		return;
	}

	global_shortcut->resource = shortcut_resource;
	global_shortcut->manager = manager;
	global_shortcut->id = strdup(id);
	global_shortcut->app_id = strdup(app_id);
	global_shortcut->description = strdup(description);
	global_shortcut->trigger_description = strdup(trigger_description);

	wl_signal_init(&global_shortcut->events.destroy);

	wl_resource_set_user_data(shortcut_resource, global_shortcut);
	wl_list_insert(&manager->resources, &global_shortcut->link);

	struct wlr_hyprland_global_shortcuts_register_shortcut_event_v1 event = {
		.shortcut = global_shortcut
	};
	wl_signal_emit_mutable(&manager->events.register_shortcut, &event);
}

static const struct hyprland_global_shortcut_v1_interface shortcut_impl = {
	.destroy = hypr_shortcut_handle_resource_destroy,
};

static const struct hyprland_global_shortcuts_manager_v1_interface shortcuts_manager_impl = {
	.destroy = hypr_global_shortcuts_handle_resource_destroy,
	.register_shortcut = hypr_global_shortcuts_handle_register_shortcut,
};

static void global_shortcuts_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id){
	struct wlr_hyprland_global_shortcuts_manager_v1 *manager = data;

	struct wl_resource *resource = wl_resource_create(client,
		&hyprland_global_shortcuts_manager_v1_interface, version, id);

	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &shortcuts_manager_impl, manager, NULL);
}  

struct wlr_hyprland_global_shortcuts_manager_v1 *wlr_hyprland_global_shortcuts_manager_v1_create(struct wl_display *display, uint32_t version){
	assert(version <= HYPRLAND_GLOBAL_SHORTCUTS_V1_VERSION);
	
	struct wlr_hyprland_global_shortcuts_manager_v1 *manager = calloc(1, sizeof(*manager));

	if (manager == NULL) {
		return NULL;
	}

	manager->global = wl_global_create(display, 
			&hyprland_global_shortcuts_manager_v1_interface, version, manager, global_shortcuts_bind);

	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}

	wl_signal_init(&manager->events.register_shortcut);
	wl_signal_init(&manager->events.destroy);

	wl_list_init(&manager->resources);

	return manager;
}

void wlr_hyprland_global_shortcuts_manager_v1_destroy(struct wlr_hyprland_global_shortcuts_manager_v1 *manager){
	if(!manager){
		return;
	}

	wl_signal_emit(&manager->events.destroy, manager);

	wl_global_destroy(manager->global);	

	struct wlr_hyprland_global_shortcut_v1 *shortcut, *tmp;
    wl_list_for_each_safe(shortcut, tmp, &manager->resources, link) {
        wl_resource_destroy(shortcut->resource);
    }

	free(manager);
}

void wlr_hyprland_global_shortcut_v1_send_pressed(
    struct wlr_hyprland_global_shortcut_v1 *shortcut, 
	uint32_t time_hi, uint32_t time_lo, uint32_t time_ns){
	if(!shortcut){
		return;
	}

	hyprland_global_shortcut_v1_send_pressed(shortcut->resource, time_hi, time_lo, time_ns);
}

void wlr_hyprland_global_shortcut_v1_send_released(
    struct wlr_hyprland_global_shortcut_v1 *shortcut, 
	uint32_t time_hi, uint32_t time_lo, uint32_t time_ns){
	if(!shortcut){
		return;
	}

	hyprland_global_shortcut_v1_send_released(shortcut->resource, time_hi, time_lo, time_ns);
}

