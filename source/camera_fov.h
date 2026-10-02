#ifndef BOZ_CAMERA_FOV_H
#define BOZ_CAMERA_FOV_H
int camera_fov_setting(int degrees);
int camera_fov_valid(float degrees);
float camera_fov_effective(float current, float baseline, int requested);
/* current/baseline are engine HALF angles; requested is the full menu angle. */
#endif
