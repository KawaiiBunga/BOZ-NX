#ifndef BOZ_HUD_CONTROLS_H
#define BOZ_HUD_CONTROLS_H
int hud_stick_atlas(unsigned width, unsigned height, const float *projection);
int hud_stick_region(float x, float y);
int hud_project_vertex(float x, float y, float z, const float *mvp,
                       const int *viewport, int render_width, int render_height,
                       float *screen_x, float *screen_y);
#endif
