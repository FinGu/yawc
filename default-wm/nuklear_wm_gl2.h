#include "../wm_api.h"
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "nuklear.h"

#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>

NK_API void nk_wm_init(void*);

NK_API struct nk_context   nk_wm_ctx_create();
NK_API void nk_wm_ctx_destroy(struct nk_context *ctx);

NK_API void                 nk_wm_font_stash_begin(struct nk_font_atlas **atlas);
NK_API void                 nk_wm_font_stash_end();
NK_API int                  nk_wm_handle_pointer_event(struct nk_context *ctx, wm_pointer_event_t *evt, int local_x, int local_y);
NK_API void                 nk_wm_render(enum nk_anti_aliasing , int max_vertex_buffer, int max_element_buffer, int width, int height, struct nk_context *ctx);
NK_API void                 nk_wm_shutdown(void);
NK_API void                 nk_wm_device_destroy(void);
NK_API void                 nk_wm_device_create(void);
NK_API void                 nk_wm_handle_grab(void);
