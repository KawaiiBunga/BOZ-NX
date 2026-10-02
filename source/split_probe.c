#include "split_probe.h"
#include <string.h>

static const struct { uint32_t vtable; const char *name; } classes[] = {
    {0, "unknown"}, {0x400798, "local-player"}, {0x401968, "remote-player"},
    {0x4069d8, "weapon-manager"}, {0x406b10, "world"},
    {SPLIT_RENDER_VTABLE_RVA, "in-game-state"}, {0x3fd1c8, "input-manager"},
    {0x3fa418, "game-network"}, {0x3e6290, "camera"}
};
static struct {
    int enabled;
    SplitPad pad[2];
    uint32_t render_state, manager, camera, weapon;
    uint64_t render_calls, camera_calls;
} probe;

const char *split_probe_kind_name(enum SplitObjectKind kind) {
    return kind > SPLIT_UNKNOWN && kind < SPLIT_OBJECT_KINDS ? classes[kind].name : "unknown";
}

/* Read only within the allocation and mapped memory, including short-lived
 * or partially initialized objects. Classification does not call game code. */
int split_probe_identify(const GuestMem *mem, uint32_t base, uint32_t address,
                         uint32_t size, SplitObject *out) {
    memset(out, 0, sizeof *out);
    if (size < 8 || size > UINT32_MAX - address || !guest_ptr(mem, address, 8)) return 0;
    guest_ld32(mem, address, &out->vtable);
    for (unsigned i = 1; i < SPLIT_OBJECT_KINDS; ++i) {
        if ((uint64_t)base + classes[i].vtable != out->vtable) continue;
        out->kind = (enum SplitObjectKind)i;
        out->address = address;
        guest_ld32(mem, address + 4, &out->identity);
        if (out->kind == SPLIT_LOCAL_PLAYER || out->kind == SPLIT_REMOTE_PLAYER ||
            out->kind == SPLIT_WEAPON_MANAGER || out->kind == SPLIT_CAMERA) {
            if (size >= 0x10) guest_ld32(mem, address + 0x0c, &out->entity);
        }
        if (out->kind == SPLIT_LOCAL_PLAYER && size >= 0x16c)
            guest_ld32(mem, address + 0x168, &out->transform);
        if (out->kind == SPLIT_REMOTE_PLAYER && size >= 0xac)
            guest_ld32(mem, address + 0xa8, &out->transform);
        if (out->kind == SPLIT_WEAPON_MANAGER) {
            /* Its init resolves components from the parent player entity.
             * +0x50 is CIsTransform, not CPlayerController; +0x60 is the
             * controller returned by the entity component lookup. */
            if (size >= 0x4c) guest_ld32(mem, address + 0x48, &out->player_entity);
            if (size >= 0x54) guest_ld32(mem, address + 0x50, &out->transform);
            if (size >= 0x64) guest_ld32(mem, address + 0x60, &out->player);
            if (size >= 0x7c) guest_ld32(mem, address + 0x78, &out->camera);
            if (size >= 0x84) guest_ld32(mem, address + 0x80, &out->camera_transform);
            if (size >= 0x1e4) guest_ld32(mem, address + 0x1e0, &out->weapon);
        }
        return 1;
    }
    return 0;
}

void split_probe_init(int enabled) {
    memset(&probe, 0, sizeof probe);
    probe.enabled = !!enabled;
}
int split_probe_enabled(void) { return probe.enabled; }
void split_probe_set_pad(unsigned player, const SplitPad *pad) {
    if (!probe.enabled || player >= 2) return;
    probe.pad[player] = *pad;
    if (!pad->connected) memset(&probe.pad[player], 0, sizeof probe.pad[player]);
}
const SplitPad *split_probe_pad(unsigned player) {
    return player < 2 ? &probe.pad[player] : NULL;
}
void split_probe_render(uint32_t state) {
    if (!probe.enabled) return;
    probe.render_state = state;
    ++probe.render_calls;
}
void split_probe_call_render(GuestCpu *frame, uint32_t original) {
    split_probe_render(frame->r[0]);
    uint32_t (*call)(uint32_t, uint32_t, uint32_t, uint32_t) =
        (void *)(uintptr_t)original;
    frame->r[0] = call(frame->r[0], frame->r[1], frame->r[2], frame->r[3]);
}
void split_probe_camera(uint32_t manager, uint32_t camera, uint32_t weapon) {
    if (!probe.enabled) return;
    probe.manager = manager; probe.camera = camera; probe.weapon = weapon;
    ++probe.camera_calls;
}
void split_probe_write(FILE *out, uint32_t live, uint32_t peak,
                       uint32_t high_water, uint32_t capacity) {
    fprintf(out, "split-probe enabled=%d render-calls=%llu camera-calls=%llu\n",
            probe.enabled, (unsigned long long)probe.render_calls,
            (unsigned long long)probe.camera_calls);
    fprintf(out, "render-state=%08x weapon-manager=%08x camera=%08x weapon=%08x\n",
            probe.render_state, probe.manager, probe.camera, probe.weapon);
    fprintf(out, "heap live=%u peak=%u high-water=%u capacity=%u\n",
            live, peak, high_water, capacity);
    for (unsigned p = 0; p < 2; ++p) {
        const SplitPad *s = &probe.pad[p];
        fprintf(out, "pad player=%u connected=%d buttons=%016llx left=%d,%d right=%d,%d\n",
                p + 1, s->connected, (unsigned long long)s->buttons,
                s->lx, s->ly, s->rx, s->ry);
    }
}
