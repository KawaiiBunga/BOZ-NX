#include "camera_controller.h"
#include "camera_fov.h"
#include <string.h>
uint32_t camera_controller_update(CameraController *state, GuestMem *mem,
    uint32_t device, uint32_t original_update, uint32_t setter, CameraCall call, void *user) {
    /* Complete the game's zoom transition before changing its rendered lens.
     * Controller baseline/current values and weapon ADS settings stay original. */
    uint32_t result = call(user, original_update, device, 0);
    uint32_t camera, current_bits, baseline_bits, actual_bits;
    if (!guest_ld32(mem, device + 0x78, &camera) || !camera ||
        !guest_ld32(mem, device + 0xc0, &current_bits) ||
        !guest_ld32(mem, device + 0xc4, &baseline_bits) ||
        !guest_ld32(mem, camera + 0x40, &actual_bits)) return result;
    float current, baseline;
    memcpy(&current, &current_bits, 4); memcpy(&baseline, &baseline_bits, 4);
    if (!camera_fov_valid(current) || !camera_fov_valid(baseline)) return result;
    state->original = (int)(baseline * 2.0f + 0.5f);
    if (!state->requested && camera != state->owned_camera) return result;
    float effective = camera_fov_effective(current, baseline, state->requested);
    uint32_t effective_bits;
    memcpy(&effective_bits, &effective, 4);
    if (effective_bits != actual_bits) call(user, setter, camera, effective_bits);
    state->owned_camera = state->requested ? camera : 0;
    return result;
}
