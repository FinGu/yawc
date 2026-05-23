#include "window_data.h"

#include <stdlib.h>

struct window_data *alloc_window_data(wm_buffer *buf){
    struct window_data *wdata = calloc(1, sizeof(struct window_data));
    
    wdata->buffer = buf;

    wdata->ctx = nk_wm_ctx_create();

    return wdata;
}

void free_window_data(struct window_data *data){
    wm_destroy_buffer(data->buffer);

    nk_wm_ctx_destroy(&data->ctx);

    free(data);
}
