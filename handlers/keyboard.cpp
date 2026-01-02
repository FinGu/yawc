#include <tuple>

#include "../utils.hpp"
#include "../wm_defs.hpp"

#include <linux/input-event-codes.h>
#include <unistd.h>

bool handle_default_keybinds(struct yawc_server* server, xkb_keysym_t sym, uint32_t modifiers){
    if (modifiers & WLR_MODIFIER_ALT &&
		sym >= XKB_KEY_XF86Switch_VT_1 &&
		sym <= XKB_KEY_XF86Switch_VT_12) {
		unsigned int vt = sym - XKB_KEY_XF86Switch_VT_1 + 1;
        wlr_session_change_vt(server->session, vt);

		return true;
	}

    switch (sym) {
    case XKB_KEY_Escape:
        wl_display_terminate(server->wl_display);
        break;
    default:
        return false;
    }

    return true;
}

void handle_keyboard_input(struct wl_listener* listener,
        void* data){

    wlr_log(WLR_DEBUG, "Handling keyboard input");

    struct yawc_keyboard* xkeyboard = wl_container_of(listener, xkeyboard, key);
    auto *server = xkeyboard->server;

    struct wlr_keyboard_key_event* event = reinterpret_cast<struct wlr_keyboard_key_event*>(data);

    uint32_t keycode = event->keycode + 8;
    
    const xkb_keysym_t sym = xkb_state_key_get_one_sym(xkeyboard->wlr_keyboard->xkb_state, keycode);

    bool handled = false;
    uint32_t modifiers = wlr_keyboard_get_modifiers(xkeyboard->wlr_keyboard);

    if ((modifiers & WLR_MODIFIER_ALT) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
            handled = handle_default_keybinds(xkeyboard->server, sym, modifiers);
    }

    utils::wake_up_from_idle(xkeyboard->server);

    if(handled){
        return; 
    }

    if(server->keybind_manager->needed()){
        server->keybind_manager->store(sym, modifiers, event->state == WL_KEYBOARD_KEY_STATE_PRESSED);

        struct yawc_bind_node *keybind;
        if((keybind = server->keybind_manager->triggered())){
            server->keybind_manager->execute_action(keybind, sym);
            return;
        }

        if(server->keybind_manager->in_sequence()){
            return;
        }
    }

    if(xkeyboard->server->wm.handle && xkeyboard->server->wm.callbacks.on_key){
        wm_keyboard_event_t kevent = wm_create_empty_keyboard_event();

        kevent.keysym = sym;
        kevent.modifiers = modifiers;
        kevent.pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;
        kevent.raw_code = keycode;

        handled = !xkeyboard->server->wm.callbacks.on_key(&kevent);

        wm_destroy_keyboard_event(&kevent);
    }

    auto *seat = xkeyboard->server->seat;

    if (!handled) {
        wlr_seat_set_keyboard(seat, xkeyboard->wlr_keyboard);
        wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
            event->state);
    }
}

void handle_keyboard_destroy(struct wl_listener* listener, void* data){
    struct yawc_keyboard* xkeyboard = wl_container_of(listener, xkeyboard, destroy);

    wl_list_remove(&xkeyboard->modifiers.link);
    wl_list_remove(&xkeyboard->key.link);
    wl_list_remove(&xkeyboard->destroy.link);
    wl_list_remove(&xkeyboard->link);

    delete xkeyboard;
}

void handle_keyboard_modifiers(struct wl_listener* listener, void* data){
    struct yawc_keyboard* xkeyboard = wl_container_of(listener, xkeyboard, modifiers);

    wlr_seat_set_keyboard(xkeyboard->server->seat, xkeyboard->wlr_keyboard);

    wlr_seat_keyboard_notify_modifiers(xkeyboard->server->seat,
            &xkeyboard->wlr_keyboard->modifiers);
}

yawc_keyboard *yawc_server::handle_keyboard(struct wlr_input_device *device){
    struct wlr_keyboard* wlr_keyboard = wlr_keyboard_from_input_device(device);

    yawc_keyboard* xkeyboard = new yawc_keyboard{};
    xkeyboard->wlr_keyboard = wlr_keyboard;
    xkeyboard->server = this;

    xkeyboard->modifiers.notify = handle_keyboard_modifiers;
    wl_signal_add(&wlr_keyboard->events.modifiers, &xkeyboard->modifiers);

    xkeyboard->key.notify = handle_keyboard_input;
    wl_signal_add(&wlr_keyboard->events.key, &xkeyboard->key);

    xkeyboard->destroy.notify = handle_keyboard_destroy;
    wl_signal_add(&wlr_keyboard->base.events.destroy, &xkeyboard->destroy);

    wlr_seat_set_keyboard(this->seat, xkeyboard->wlr_keyboard);
    wl_list_insert(&this->keyboards, &xkeyboard->link);

    return xkeyboard;
}

void yawc_server::load_keyboard_cfg(yawc_keyboard *keyboard){
    yawc_keyboard_config config;

    auto device = &keyboard->wlr_keyboard->base;

	if(this->config->input_configs.count(device->name)){
		config = std::get<yawc_keyboard_config>(this->config->input_configs[device->name]);
	} else{
		config = this->config->default_keyboard_config;
	}

    if(!config.enabled){
        return;
    }

    struct xkb_rule_names rules; 

    rules.layout = config.xkb_layout.has_value() ? config.xkb_layout->c_str() : nullptr;
    rules.model = config.xkb_model.has_value() ? config.xkb_model->c_str() : nullptr;
    rules.options = config.xkb_options.has_value() ? config.xkb_options->c_str() : nullptr;
    rules.rules = config.xkb_rules.has_value() ? config.xkb_rules->c_str() : nullptr;
    rules.variant = config.xkb_variant.has_value() ? config.xkb_variant->c_str() : nullptr;
        
	wlr_keyboard_set_repeat_info(keyboard->wlr_keyboard, 
            config.repeat_rate.value_or(25), 
            config.repeat_delay.value_or(600));

    struct xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap* keymap = xkb_keymap_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(keyboard->wlr_keyboard, keymap);

    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
}
