#include "wm_api.h"
#include "nuklear_wm_gl2.h"

struct task_switcher_data{
    struct nk_context ctx;
    wm_toplevel *selected;

    wm_toplevel **toplevels;
    size_t toplevel_amount;

    int current_index;

    wm_buffer *buffer;
    bool initialized;

    bool active;
}; 

void draw_task_switcher(void *data);

void draw_switcher_element(struct task_switcher_data *wdata, wm_toplevel *cur);

void destroy_task_switcher(struct task_switcher_data *ts_data);

wm_buffer *create_task_switcher(struct task_switcher_data *ts_data);
