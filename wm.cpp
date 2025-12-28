#include "wm_defs.hpp"
#include "utils.hpp"

#include <dlfcn.h>
#include <unistd.h>

void yawc_server::unload_wm(){
    if(!wm.handle){
        return;
    }

    wm.unregister_fn();

    dlclose(wm.handle);

    wm.handle = nullptr;
    wm.callbacks = {0};
}

void reload_toplevels(struct yawc_server *sv){
    auto *wm = &sv->wm;

    if(wm->callbacks.on_map){
        std::vector<struct yawc_toplevel *> existing_toplevels;

        struct yawc_toplevel *pos;
        wl_list_for_each(pos, &sv->toplevels, link){
            existing_toplevels.push_back(pos);
        } 

        //in case the wm messes with the order ( with focus_toplevel )
        //we need to keep a snapshot list
        for(auto el: existing_toplevels){
            wm_toplevel topl{el};
            wm->callbacks.on_map(&topl); 
        }
    }
}

void yawc_server::load_wm(const char *path){
    if(!path){
        return;
    }

    this->unload_wm();

    bool (*wm_register_plugin_impl)(const wm_callbacks_t *cbs, void *user_data);
    void (*wm_unregister_plugin_impl)();

    void *handle = dlopen(path, RTLD_LAZY);

    if(!handle){
        wlr_log(WLR_ERROR, "Failed to load the window manager: %s", dlerror());
        return;
    }

    *(void**)(&wm_register_plugin_impl) = dlsym(handle, "wm_register");
    *(void**)(&wm_unregister_plugin_impl) = dlsym(handle, "wm_unregister");

    wm_callbacks_t callbacks = {0};

    if(!wm_register_plugin_impl(&callbacks, NULL)){
        wlr_log(WLR_ERROR, "Failed to initialize wm");
        dlclose(handle);
        return;
    }

    wm.handle = handle;
    wm.callbacks = callbacks;
    wm.register_fn = wm_register_plugin_impl;
    wm.unregister_fn = wm_unregister_plugin_impl;
    wm.hash = utils::hash_file_fnv1a(path);
  
    reload_toplevels(this);
}

