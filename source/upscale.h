#ifndef BOZ_UPSCALE_H
#define BOZ_UPSCALE_H
#include <stdint.h>
void upscale_initialize(void *context, int render_height, int output_height);
void upscale_forget(void *context);
int upscale_active(void);
int upscale_render_width(int fallback);
int upscale_render_height(int fallback);
void upscale_present(int output_width, int output_height);
void upscale_resume(void);
void upscale_suspend(void);
void upscale_draw_texture(int output_width, int output_height);
#endif
