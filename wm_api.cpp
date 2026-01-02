#include <drm/drm_fourcc.h>
#include <gbm.h>
#include <GLES2/gl2.h>
#include <memory>

#include "wm_api.h"
#include "wm_defs.hpp"
#include "window_ops.hpp"

yawc_server *wm_server;

WM_API void wm_plugin_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    putchar('\n');
    va_end(ap);
}

WM_API void wm_focus_toplevel(wm_toplevel *t){
    if(!t){
        return;
    }

    utils::focus_toplevel(t->toplevel);
}

WM_API void wm_raise_toplevel(wm_toplevel *t){
    if(!t){
        return;
    }

    wlr_scene_node_raise_to_top(&t->toplevel->scene_tree->node);
}

WM_API void wm_lower_toplevel(wm_toplevel *t){
    if(!t){
        return;
    }

    wlr_scene_node_lower_to_bottom(&t->toplevel->scene_tree->node);
}

WM_API wm_id_t wm_toplevel_get_id(wm_toplevel *t) {
    if(!t){
        return -1;
    }

    return t->toplevel->id;
}

WM_API wm_toplevel *wm_try_get_toplevel_from_node(wm_node *n) {
    if(!n){
        return nullptr;
    }

    auto *node = n->node;

    if(node->type != WLR_SCENE_NODE_BUFFER){
        return nullptr;
    }

    struct wlr_scene_buffer* scene_buffer = wlr_scene_buffer_from_node(node);
    struct wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);

    if (!scene_surface) {
        return nullptr;
    }

    struct wlr_scene_tree* tree = node->parent;
    while (tree != NULL && tree->node.data == NULL) {
        tree = tree->node.parent;
    }

    if (tree == NULL) {
        return nullptr;
    }

    struct yawc_scene_descriptor *desc = scene_descriptor_try_get(&tree->node, YAWC_SCENE_DESC_VIEW);

    if(!desc){
        return nullptr;
    }

    return wm_create_toplevel(static_cast<yawc_toplevel*>(desc->parent));
}

WM_API wm_buffer *wm_try_get_buffer_from_node(wm_node *n) {
    if(!n){
        return nullptr;
    }

    auto *node = n->node;

    if(node->type != WLR_SCENE_NODE_BUFFER){
        return nullptr;
    }

    struct wlr_scene_buffer* scene_buffer = wlr_scene_buffer_from_node(node);

    wm_buffer *buffer = static_cast<wm_buffer*>(scene_buffer->node.data);

    return buffer ? buffer : nullptr;
}


WM_API wm_node_at_coords_t *wm_try_get_node_at_coords(double x, double y){
        auto [node, input_on_node] = utils::desktop_node_at(wm_server, x, y);

        if(!node){
            return nullptr;
        }

        wm_node_at_coords_t *coords = new wm_node_at_coords_t;
        coords->global_x = x;
        coords->global_y = y;

        coords->node = new wm_node{node};

        coords->local_x = input_on_node.x;
        coords->local_y = input_on_node.y;
        return coords;
}

WM_API void wm_unref_node_at_coords(wm_node_at_coords_t *coords){
    if(!coords){
        return;
    }

    delete coords->node;

    delete coords;
}

WM_API uint32_t wm_try_get_resize_grip(wm_node *n, wm_toplevel **t){
    if(!n){
        return WM_RESIZE_EDGE_INVALID;
    }

    auto *node = n->node;

    if(!node || node->type != WLR_SCENE_NODE_RECT){
        return WM_RESIZE_EDGE_INVALID;
    }

    auto desc = 
        scene_descriptor_try_get(node, YAWC_SCENE_DESC_RESIZE_GRIP);

    if(!desc){
        return WM_RESIZE_EDGE_INVALID;
    }

    if(t){
        *t = wm_create_toplevel(reinterpret_cast<struct yawc_toplevel*>(desc->parent));
    }

    return (uint32_t)(uintptr_t)desc->data;
}

WM_API int wm_get_amount_of_toplevels(){
    return wl_list_length(&wm_server->toplevels);

    /*
    yawc_toplevel *toplevel;

    size_t count = 0;
    wl_list_for_each(toplevel, &wm_server->toplevels, link){
        if (toplevel->mapped) { //has to be done this way because of X
            count++;
        }
    }
    return count;
*/
}

WM_API void wm_foreach_toplevel(wm_toplevel_iter_cb cb, void *user) {
    struct yawc_toplevel *toplevel;
    wl_list_for_each(toplevel, &wm_server->toplevels, link){
        wm_toplevel fnin;
        fnin.toplevel = toplevel;

        cb(&fnin, user);
    }
}

WM_API float wm_get_output_render_scale(wm_output *output){
    if(!output || !output->output || !output->output->wlr_output){
        return 1;
    }

    return output->output->wlr_output->scale;
}

WM_API void wm_begin_move(wm_toplevel *t) {
    if(!t){
        return;
    }

    begin_move(t->toplevel);
}

WM_API void wm_begin_resize(wm_toplevel *t, uint32_t edge_bits){
    if(!t){
        return;
    }

    begin_resize(t->toplevel, edge_bits);
}

WM_API void wm_set_cursor(const char* name){
    wlr_cursor_set_xcursor(wm_server->cursor, wm_server->cursor_mgr, name);
}

WM_API const char *wm_get_cursor_name_from_edges(uint32_t bits){
    return wlr_xcursor_get_resize_name((enum wlr_edges)bits);
}

WM_API void wm_cancel_window_op() {
    wm_server->reset_cursor_mode();
}

WM_API void wm_set_toplevel_position(wm_toplevel *t, int x, int y) {
    if(!t){
        return;
    }

    yawc_toplevel *toplevel = t->toplevel;

    wlr_scene_node_set_position(&toplevel->scene_tree->node, x, y);
}

WM_API void wm_set_toplevel_geometry(wm_toplevel *t, wm_box_t geo) {
    if(!t){
        return;
    }

    yawc_toplevel *toplevel = t->toplevel;

    wlr_scene_node_set_position(&toplevel->scene_tree->node, geo.x, geo.y);

    wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, geo.width, geo.height);
}

WM_API wm_box_t wm_restore_toplevel_geometry(wm_toplevel *t){
    if(!t){
        return {};
    }

    auto geo = t->toplevel->reset_state(); 

    return wm_box_t{geo.x, geo.y, geo.width, geo.height};
}

WM_API wm_box_t wm_get_buffer_geometry(wm_buffer *b){
    if(!b){
        return {};
    }

    return b->box;
}

WM_API void wm_unref_output(wm_output *o){
    delete o;
}

WM_API void wm_unref_toplevel(wm_toplevel *t){
    delete t;
}

//X impl
/*
WM_API wm_toplevel **wm_get_toplevels(size_t *size){
    struct yawc_toplevel *toplevel;

    size_t count = 0;
    wl_list_for_each(toplevel, &wm_server->toplevels, link){
        if (toplevel->mapped) {
            count++;
        }
    }

    if(size){
        *size = count;
    }

    if(!count){
        return nullptr;
    }

    wm_toplevel **list = new wm_toplevel*[count];

    count = 0;

    wl_list_for_each(toplevel, &wm_server->toplevels, link){
        if (toplevel->mapped) {
            list[count++] = wm_create_toplevel(toplevel);
        }
    }

    return list;
}
*/

WM_API wm_toplevel **wm_get_toplevels(size_t *size){
    struct yawc_toplevel *toplevel;

    size_t count = wm_get_amount_of_toplevels();
    
    if(size){
        *size = count;
    }

    if(!count){
        return nullptr;
    }

    wm_toplevel **list = new wm_toplevel*[count];

    count = 0;

    wl_list_for_each(toplevel, &wm_server->toplevels, link){
        list[count++] = wm_create_toplevel(toplevel);
    }

    return list;
}

WM_API void wm_unref_toplevels(wm_toplevel **t, size_t size){
    if(!t){
        return;
    }

    for(size_t i = 0; i < size; ++i){
        wm_unref_toplevel(t[i]);
    }

    delete[] t;
}

WM_API const char *wm_get_toplevel_title(wm_toplevel *t){
    if(!t || !t->toplevel){
        return nullptr;
    }
    
    auto *toplevel = t->toplevel;

    return toplevel->title.c_str();
}

WM_API uint64_t wm_get_toplevel_id(wm_toplevel *t){
    if(!t){
        return -1;
    }

    return t->toplevel->id;
}

WM_API void wm_hide_toplevel(wm_toplevel *t) {
    if(!t){
        return;
    }

    yawc_toplevel *toplevel = t->toplevel;

    wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
    wlr_foreign_toplevel_handle_v1_set_minimized(toplevel->foreign_handle, true);
    
    auto *tmp_toplevel = wm_get_focused_toplevel();

    if(!tmp_toplevel){
        return;
    }

    if(t->toplevel == tmp_toplevel->toplevel){
        wlr_seat_keyboard_clear_focus(wm_server->seat);
    }

    wm_unref_toplevel(tmp_toplevel);

    t->toplevel->hidden = true;
}

WM_API void wm_unhide_toplevel(wm_toplevel *t) {
    if(!t){
        return;
    }

    yawc_toplevel *toplevel = t->toplevel;

    if(t->toplevel->hidden){
        wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);
        wlr_foreign_toplevel_handle_v1_set_minimized(toplevel->foreign_handle, false);

        t->toplevel->hidden = false;
    }
}

WM_API void wm_close_toplevel(wm_toplevel *t) {
    if(!t){
        return;
    }

    wlr_xdg_toplevel_send_close(t->toplevel->xdg_toplevel);
}

wlr_scene_node *create_grip_for_toplevel(wm_grip_visual grip, yawc_toplevel *toplevel, 
        int width, int height, uint32_t bits){
    struct wlr_scene_node *node = nullptr;

    if(grip.type == WM_GRIP_VISUAL_NONE){
        return node;
    }
    
    if(grip.type == WM_GRIP_VISUAL_COLOR){
        auto rect = wlr_scene_rect_create(toplevel->scene_tree, width, height, grip.color);
        node = &rect->node;
    } else{
        if(!grip.buffer){
            return nullptr; 
        }

        auto buf = wlr_scene_buffer_create(toplevel->scene_tree, grip.buffer->buffer);
        node = &buf->node;
    }

    if(node){
        scene_descriptor_assign(node, YAWC_SCENE_DESC_RESIZE_GRIP, toplevel, 
                (void*)(uintptr_t)bits);
    }

    return node;
}

void update_toplevel_resize_grip(
        wm_toplevel *toplevel,
        struct wlr_scene_node **node, 
        int x, int y, 
        int width, int height, 
        wm_grip_render_cb render_callback,
        uint32_t bits,
        void *user_data){

    wm_grip_visual grip = render_callback(toplevel, width, height, bits, user_data);

    auto *ytoplevel = toplevel->toplevel;

    if (!*node) {
        *node = create_grip_for_toplevel(grip, ytoplevel, width, height, bits);
    } else {
        bool needs_destroying = false;

        needs_destroying = (*node)->type == WLR_SCENE_NODE_BUFFER && grip.type != WM_GRIP_VISUAL_BUFFER;
        needs_destroying = (*node)->type == WLR_SCENE_NODE_RECT && grip.type != WM_GRIP_VISUAL_COLOR;
        needs_destroying = grip.type == WM_GRIP_VISUAL_NONE;

        if(needs_destroying){
            wlr_scene_node_destroy(*node); 
            *node = create_grip_for_toplevel(grip, ytoplevel, width, height, bits);
        } else if (grip.type == WM_GRIP_VISUAL_COLOR) {
            auto *rect = wlr_scene_rect_from_node(*node);
            wlr_scene_rect_set_size(rect, width, height);
            wlr_scene_rect_set_color(rect, grip.color);
        }  else if (grip.type == WM_GRIP_VISUAL_BUFFER && grip.buffer) {
            auto *scene_buf = wlr_scene_buffer_from_node(*node);
            wlr_scene_buffer_set_buffer(scene_buf, grip.buffer->buffer);
        }
    }

    if(!*node){
        return;
    }

    wlr_scene_node_set_position(*node, x, y);
    wlr_scene_node_raise_to_top(*node);
}

WM_API void wm_configure_toplevel_resize_grips(
    wm_toplevel *t, 
    int off_x, int off_y, 
    int width, int height, 
    int grip_thickness, 
    wm_grip_render_cb render_cb,
    void *user_data){
    if (!t) {
        return;
    }

    auto *toplevel = t->toplevel;

    if (!toplevel->scene_tree) {
        return; 
    }

    update_toplevel_resize_grip(t,
        &toplevel->resize_grips[4], 
        off_x - grip_thickness, 
        off_y - grip_thickness, 
        grip_thickness, 
        grip_thickness,
        render_cb,
        WM_RESIZE_EDGE_TOP | WM_RESIZE_EDGE_LEFT,
        user_data);

    update_toplevel_resize_grip(t,
        &toplevel->resize_grips[5], 
        off_x + width, 
        off_y - grip_thickness, 
        grip_thickness, 
        grip_thickness,
        render_cb,
        WM_RESIZE_EDGE_TOP | WM_RESIZE_EDGE_RIGHT,
        user_data);
    
    update_toplevel_resize_grip(t, 
        &toplevel->resize_grips[6], 
        off_x - grip_thickness, 
        off_y + height, 
        grip_thickness, 
        grip_thickness,
        render_cb,
        WM_RESIZE_EDGE_BOTTOM | WM_RESIZE_EDGE_LEFT,
        user_data);

    update_toplevel_resize_grip(t,
        &toplevel->resize_grips[7], 
        off_x + width, 
        off_y + height, 
        grip_thickness, 
        grip_thickness,
        render_cb,
        WM_RESIZE_EDGE_BOTTOM | WM_RESIZE_EDGE_RIGHT,
        user_data);

    update_toplevel_resize_grip(t,
        &toplevel->resize_grips[0], 
        off_x, 
        off_y - grip_thickness, 
        width, 
        grip_thickness,
        render_cb,
        WM_RESIZE_EDGE_TOP,
        user_data);

    update_toplevel_resize_grip(t,
        &toplevel->resize_grips[1], 
        off_x, 
        off_y + height, 
        width, 
        grip_thickness,
        render_cb,
        WM_RESIZE_EDGE_BOTTOM,
        user_data);

    update_toplevel_resize_grip(t,
        &toplevel->resize_grips[2], 
        off_x - grip_thickness, 
        off_y, 
        grip_thickness, 
        height,
        render_cb,
        WM_RESIZE_EDGE_LEFT,
        user_data);

    update_toplevel_resize_grip(t,
        &toplevel->resize_grips[3], 
        off_x + width, 
        off_y, 
        grip_thickness, 
        height,
        render_cb,
        WM_RESIZE_EDGE_RIGHT,
        user_data);

   toplevel->has_resize_grips = true;
}
 
WM_API wm_box_t wm_get_output_geometry(wm_output *o) {
    wm_box_t out = {0};

    if(!o || !o->output || !wm_server->output_layout){
        return out;
    }

    struct wlr_box box;
    wlr_output_layout_get_box(wm_server->output_layout, o->output->wlr_output, &box);

    out.x = box.x;
    out.y = box.y;
    out.width = box.width;
    out.height = box.height;

    return out;
}

WM_API wm_output *wm_get_focused_output() {
    auto *wlr_output = wlr_output_layout_output_at(wm_server->output_layout, wm_server->cursor->x, wm_server->cursor->y);

    if(!wlr_output){
        return nullptr;
    }
    
    yawc_output *output = static_cast<yawc_output*>(wlr_output->data);

    return new wm_output{output};
}

WM_API wm_output *wm_get_output_of_toplevel(wm_toplevel *t) {
    if(!t){
        return nullptr;
    }

    auto *output = utils::get_output_of_toplevel(t->toplevel);

    if(!output){
        return nullptr;
    }

    return new wm_output{output};
}

WM_API wm_box_t wm_get_toplevel_geometry(wm_toplevel *t) {
    wm_box_t box = {0};

    if(!t){
        return box;
    }

    auto geometry = t->toplevel->xdg_toplevel->base->geometry;

    box.width = geometry.width;
    box.height = geometry.height;
    box.x = geometry.x;
    box.y = geometry.y;

    return box;
}

WM_API wm_box_t wm_get_output_usable_area(wm_output *o) {
    wm_box_t box = {0};

    if(!o){
        return box;
    }

    auto area = utils::get_usable_area_of_output(o->output);
    box.x = area.x;
    box.y = area.y;
    box.width = area.width;
    box.height = area.height;
    
    return box;
}

WM_API void wm_set_toplevel_fullscreen(wm_toplevel *t, bool f){
    auto *toplevel = t->toplevel;
    toplevel->set_fullscreen(f);
}

WM_API void wm_set_toplevel_maximized(wm_toplevel *t, bool m){
    auto *toplevel = t->toplevel;

    toplevel->save_state();

    toplevel->maximized = m;

    wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, m);
}

WM_API bool wm_toplevel_is_fullscreen(wm_toplevel *t){
    if(!t){
        return false;
    }

    return t->toplevel->fullscreen;
}

WM_API bool wm_toplevel_is_maximized(wm_toplevel *t) {
    if(!t){
        return false;
    }

    return t->toplevel->maximized;
}


WM_API bool wm_toplevel_is_hidden(wm_toplevel *t){
    if(!t){
        return false;
    }

    return t->toplevel->hidden;
}

WM_API bool wm_toplevel_is_mapped(wm_toplevel *t) {
    if(!t || !t->toplevel){
        return false;
    }

    return t->toplevel->mapped;
}

WM_API bool wm_toplevel_is_csd(wm_toplevel *t) {
    if(!t || !t->toplevel){
        return true;
    }

    auto *decoration = t->toplevel->decoration;

    return !decoration || !decoration->is_ssd;
}

WM_API bool wm_toplevel_wants_maximize(wm_toplevel *t){
    if(!t || !t->toplevel){
        return false;
    }

    auto *toplevel = t->toplevel;

    if(!toplevel->xdg_toplevel){
        return false;
    }

    return toplevel->xdg_toplevel->requested.maximized;
}

WM_API bool wm_toplevel_wants_fullscreen(wm_toplevel *t){
    if(!t || !t->toplevel){
        return false;
    }

    auto *toplevel = t->toplevel;

    if(!toplevel->xdg_toplevel){
        return false;
    }

    return toplevel->xdg_toplevel->requested.fullscreen;
}

WM_API wm_buffer* wm_create_buffer(int width, int height, bool cpu_buffer) {
    struct wm_buffer *s = new wm_buffer{}; 

    s->toplevel = nullptr;

    //s->scene_buffer = nullptr;
    s->box = wm_box_t{.width = width, .height = height};

    s->mapped = false;

    static uint64_t mod = DRM_FORMAT_MOD_INVALID;
    static struct wlr_drm_format synth = {0};

    synth.format = DRM_FORMAT_ARGB8888;
    synth.len = 1;
    synth.modifiers = &mod;

    struct wlr_buffer *buf = wlr_allocator_create_buffer(
            cpu_buffer ? wm_server->shm_allocator : wm_server->allocator, 
        width, height, &synth);

    if(!buf){
        delete s;
        return nullptr;
    }

    s->buffer = buf;
    return s;
}

WM_API void wm_destroy_buffer(wm_buffer *b) {
    if(!b){
        return;
    }

    b->toplevel = nullptr;
    wlr_buffer_drop(b->buffer);

    delete b;
}

//only available for cpu buffers
WM_API wm_buffer_data wm_lock_buffer_data(wm_buffer *buffer){
    void *data = nullptr;
    uint32_t format;
    size_t stride;
    const uint32_t flags = WLR_BUFFER_DATA_PTR_ACCESS_WRITE | WLR_BUFFER_DATA_PTR_ACCESS_READ;
    
    bool ok = wlr_buffer_begin_data_ptr_access(buffer->buffer, flags, &data, &format, &stride);
    
    if(!ok){
        return {};
    }
   
    wm_buffer_data out;

    out.ptr = data;
    out.format = format;
    out.stride = stride;

    return out;
}

WM_API void wm_release_buffer_data(wm_buffer *buffer){
    //this is almost useless, the shm allocator doesn't do anything when ending access
    wlr_buffer_end_data_ptr_access(buffer->buffer);
}

//returns old buf
WM_API wm_buffer *wm_attach_overlay(const char *name, wm_buffer *buffer, int x, int y) {
    wm_buffer *old_buffer = nullptr;
    wlr_scene_buffer *scene_buffer;
    auto &overlays = wm_server->overlays;

    auto it = overlays.find(name);

    if(it != overlays.end()){
        scene_buffer = it->second;

        old_buffer = static_cast<wm_buffer*>(it->second->node.data);
        
        wlr_scene_buffer_set_buffer_with_damage(scene_buffer, buffer->buffer, nullptr);
    } else{
        scene_buffer = wlr_scene_buffer_create(wm_server->layers.overlay, buffer->buffer);
    }

    wlr_scene_node_set_position(&scene_buffer->node, x, y);

    wlr_scene_node_raise_to_top(&scene_buffer->node);

    wlr_scene_buffer_set_dest_size(scene_buffer, buffer->box.width, buffer->box.height);

    scene_buffer->node.data = buffer;

    overlays[name] = scene_buffer;

    return old_buffer;
}

WM_API wm_buffer *wm_unattach_overlay(const char *name) {
    auto &overlays = wm_server->overlays;

  	auto it = overlays.find(name);

    if(it == overlays.end()){
        return nullptr;
    }

    wm_buffer *buf = static_cast<wm_buffer*>(it->second->node.data);

    wlr_scene_node_destroy(&it->second->node);

    overlays.erase(it);

    return buf;
}

//returns the old buffer or nullptr
//buf passed must be valid
WM_API wm_buffer *wm_toplevel_attach_buffer(wm_toplevel *toplevel, const char *name, 
        wm_buffer *buffer, int x, int y) {
    wm_buffer *old_buffer = nullptr;
    struct wlr_scene_buffer *cur_scene_buf;

    auto ytoplevel = toplevel->toplevel;

    auto& buffers = ytoplevel->buffers;

    auto it = buffers.find(name);

    if(it != buffers.end()){
        cur_scene_buf = it->second;

        old_buffer = static_cast<wm_buffer*>(it->second->node.data);

        wlr_scene_buffer_set_buffer_with_damage(cur_scene_buf, buffer->buffer, nullptr);
    } else{
        cur_scene_buf = wlr_scene_buffer_create(ytoplevel->scene_tree, buffer->buffer);
    }

    wlr_scene_node_set_position(&cur_scene_buf->node, x, y);

    wlr_scene_buffer_set_dest_size(cur_scene_buf, buffer->box.width, buffer->box.height);

    cur_scene_buf->node.data = buffer;

    buffer->toplevel = toplevel->toplevel;

    buffers[name] = cur_scene_buf;

    return old_buffer;
}

WM_API wm_buffer *wm_toplevel_unattach_buffer(wm_toplevel *toplevel, const char *name) {
    auto &buffers = toplevel->toplevel->buffers;

  	auto it = buffers.find(name);

    if(it == buffers.end()){
        return nullptr;
    }

    wm_buffer *buf = static_cast<wm_buffer*>(it->second->node.data);

    wlr_scene_node_destroy(&it->second->node);

    buffers.erase(it);

    return buf;
}


WM_API bool wm_render_fn_to_buffer(wm_buffer *buffer, wm_render_cb cb, void *user_data) {
    std::unique_ptr<wm_buffer, void(*)(wm_buffer*)> tmp_buf(nullptr, wm_destroy_buffer);

    if (!buffer || !buffer->buffer) {
        wm_buffer* raw_tmp = wm_create_buffer(1, 1, false); //allocate tmp buf to have a viable gl context
    
        tmp_buf.reset(raw_tmp);
    
        buffer = tmp_buf.get();
    }

    struct wlr_renderer *renderer = wm_server->renderer;
    
    struct wlr_buffer_pass_options pass_opts = {0};
    pass_opts.timer = NULL;
    pass_opts.color_transform = NULL;
    pass_opts.signal_timeline = NULL;

    struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(renderer, buffer->buffer, &pass_opts);

    if (!pass) {
        return false;
    }

    if(cb){
        cb(user_data);
    } 

    if (!wlr_render_pass_submit(pass)) {
        return false;
    }

    return true;
}

WM_API void wm_toplevel_attach_state(wm_toplevel *toplevel, void *data){
    if(!toplevel){
        return;
    }

    toplevel->toplevel->wm_state = data;
}

WM_API void *wm_get_toplevel_state(wm_toplevel *toplevel){
    if(!toplevel){
        return nullptr;
    }

    return toplevel->toplevel->wm_state;
}

WM_API wm_toplevel *wm_get_toplevel_of_buffer(wm_buffer *b){
    if(!b){
        return nullptr;
    }

    auto *toplevel = b->toplevel;

    if(!toplevel){
        return nullptr;
    }

    return wm_create_toplevel(toplevel);   
}

WM_API wm_toplevel* wm_get_next_toplevel(struct wm_toplevel *cur){
    struct wl_list *list_head = &wm_server->toplevels;

    if (wl_list_empty(list_head)) {
        return nullptr;
    }

    if(!cur){
        yawc_toplevel *toplevel;

        auto *out_toplevel = wl_container_of(list_head->prev, toplevel, link);

        return wm_create_toplevel(out_toplevel);
    }

    auto *next_link = cur->toplevel->link.prev;
    if(next_link == list_head){
        next_link = next_link->prev;
    }

    auto *toplevel = wl_container_of(next_link, cur->toplevel, link);

    return wm_create_toplevel(toplevel);
}

WM_API wm_toplevel *wm_get_focused_toplevel(){
    yawc_toplevel *toplevel;

    struct wl_list *list_head = &wm_server->toplevels;
    
    if(wl_list_empty(list_head)){
        return nullptr;
    }
    
    auto *out_toplevel = wl_container_of(list_head->prev, toplevel, link);
    
    return wm_create_toplevel(out_toplevel);
}
