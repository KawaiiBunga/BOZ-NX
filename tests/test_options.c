#include "input_tuning.h"
#include "display_options.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
    assert(aim_axis(0.8f, 250) == 2.0f);
    assert(aim_axis(-0.8f, 250) == -2.0f);
    assert(aim_axis(0.8f, 50) == 0.4f);
    assert(aim_axis(0.8f, 999) == 2.0f);
    assert(aim_axis(0.8f, -1) == 0.4f);
    assert(aim_axis(-0.0f, 100) == 0.0f && signbit(aim_axis(-0.0f, 100)));
    assert(render_height_valid(541) == 720);
    assert(render_width_for_height(540) == 960);
    assert(render_width_for_height(360) == 640);
    assert(render_height_valid(1080) == 1080);
    assert(render_width_for_height(1080) == 1920);
    const int heights[] = {360, 540, 720, 1080};
    for (unsigned i = 0; i < sizeof heights / sizeof heights[0]; ++i) {
        int height = heights[i];
        int width = render_width_for_height(height);
        assert(render_coordinate(1280, width, 1280) == width);
        assert(render_coordinate(720, height, 720) == height);
        assert(render_coordinate(640, width, 1280) == width / 2);
        assert(render_coordinate(-1280, width, 1280) == -width);
    }
    puts("PASS: 250% aim scaling, clamping, default identity and render coordinate mapping");
}
