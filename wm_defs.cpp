#include "utils.hpp"
#include "wm_api.h"
#include "wm_defs.hpp"

wm_keyboard_event_t wm_create_empty_keyboard_event(){
        return wm_keyboard_event_t {};
}

wm_toplevel *wm_create_toplevel(yawc_toplevel *backer){
        auto *tp = new wm_toplevel;
        tp->toplevel = backer;
        return tp;
}

wm_pointer_event_t wm_create_pointer_event(yawc_server *sv, uint32_t button, bool pressed){
        auto evt {wm_pointer_event_t{}};

        evt.global_x = sv->cursor->x;
        evt.global_y = sv->cursor->y;

        evt.button = button;
        evt.pressed = pressed;

        return evt;
}

wm_toplevel_request_event_t wm_create_toplevel_request_event(wm_toplevel *toplevel, wm_toplevel_request_type_t type, void *data){
        auto evt{wm_toplevel_request_event_t{}};

        evt.toplevel = toplevel;
        evt.data = data; 
        evt.type = type;

        return evt;
}

void wm_destroy_pointer_event(wm_pointer_event_t *ev){}
void wm_destroy_keyboard_event(wm_keyboard_event_t *ev){}

void wm_destroy_toplevel_request_event(wm_toplevel_request_event_t *ev){
    if(!ev){
        return;
    }

    wm_unref_toplevel(ev->toplevel);
}

