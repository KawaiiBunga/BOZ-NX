#include "camera_fov.h"
#include <math.h>
int camera_fov_setting(int degrees) {
    if (degrees <= 0) return 0; /* Original game FOV. */
    return degrees < 60 ? 60 : degrees > 110 ? 110 : degrees;
}
int camera_fov_valid(float degrees) {
    return isfinite(degrees) && degrees >= 0.5f && degrees < 89.5f;
}
float camera_fov_effective(float current, float baseline, int requested) {
    int target = camera_fov_setting(requested);
    if (!target || !camera_fov_valid(current) || !camera_fov_valid(baseline)) return current;
    if (current == baseline) return (float)target * 0.5f;
    /* Preserve the game's ADS magnification, expressed as a focal-length
     * ratio, rather than adding a constant angle to every zoom level. */
    /* The actual camera setter computes extent / (2*tan(m_fov*pi/180)):
     * m_fov is already a half angle, confirmed by the game's radians literal. */
    const float radians = 0.017453292519943296f;
    float zoom = tanf(current * radians) / tanf(baseline * radians);
    float result = atanf(zoom * tanf((float)target * 0.5f * radians)) / radians;
    return result > 85.0f ? 85.0f : result;
}
