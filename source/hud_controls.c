#include "hud_controls.h"
#include <math.h>
int hud_stick_atlas(unsigned width, unsigned height, const float *projection) {
    /* The HUD atlas is shared with pause/splat art. Require its dimensions and
     * an orthographic draw, never a transient GL texture name. World geometry
     * using a texture of the same dimensions must remain untouched. */
    return width == 1024 && height == 512 && projection[3] == 0.0f &&
           projection[7] == 0.0f && projection[11] == 0.0f && projection[15] == 1.0f;
}
int hud_stick_region(float x, float y) {
    return y >= 455.0f && y <= 652.0f &&
           ((x >= 55.0f && x <= 250.0f) || (x >= 775.0f && x <= 968.0f));
}
int hud_project_vertex(float x, float y, float z, const float *mvp,
                       const int *vp, int width, int height, float *sx, float *sy) {
    float cx = mvp[0]*x + mvp[4]*y + mvp[8]*z + mvp[12];
    float cy = mvp[1]*x + mvp[5]*y + mvp[9]*z + mvp[13];
    float cw = mvp[3]*x + mvp[7]*y + mvp[11]*z + mvp[15];
    if (!isfinite(cw) || cw == 0.0f || width <= 0 || height <= 0) return 0;
    *sx = (vp[0] + (cx/cw * 0.5f + 0.5f)*vp[2]) * 1280.0f / width;
    *sy = (height - (vp[1] + (cy/cw * 0.5f + 0.5f)*vp[3])) * 720.0f / height;
    return isfinite(*sx) && isfinite(*sy);
}
