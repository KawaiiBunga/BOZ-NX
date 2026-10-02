#include "display_options.h"
#include <stdint.h>
#include <limits.h>
int render_height_valid(int height) { return height == 360 || height == 540 || height == 1080 ? height : 720; }
int render_width_for_height(int height) { return render_height_valid(height) * 16 / 9; }
int render_coordinate(int value, int extent, int logical_extent) {
    int64_t result = (int64_t)value * extent / logical_extent;
    return result > INT_MAX ? INT_MAX : result < INT_MIN ? INT_MIN : (int)result;
}
