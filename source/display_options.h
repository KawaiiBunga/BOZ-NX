#ifndef BOZ_DISPLAY_OPTIONS_H
#define BOZ_DISPLAY_OPTIONS_H
#ifdef __cplusplus
extern "C" {
#endif
/* The game keeps its 1280x720 layout; only the presentation buffer changes. */
int render_height_valid(int height);
int render_width_for_height(int height);
int render_coordinate(int value, int extent, int logical_extent);
#ifdef __cplusplus
}
#endif
#endif
