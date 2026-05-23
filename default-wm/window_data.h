#include "wm_api.h"
#include "nuklear_wm_gl2.h"

#include <time.h>

struct window_data{
    wm_buffer *buffer;
    struct nk_context ctx;
    
    struct timespec last_left_click; 
    int last_click_x;
    int last_click_y;
};

struct window_data *alloc_window_data(wm_buffer *buf);
void free_window_data(struct window_data *data);
