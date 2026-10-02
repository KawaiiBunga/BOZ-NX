#ifndef BOZ_SPLIT_PROBE_H
#define BOZ_SPLIT_PROBE_H
#include <stdint.h>
#include <stdio.h>
#include "guest.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Supported v1.0.11 image only. All addresses are RVAs, never heap pointers. */
#define SPLIT_RENDER_RVA 0x193060u
#define SPLIT_RENDER_LITERAL_RVA 0x193270u
#define SPLIT_RENDER_LITERAL 0x002d47a4u
#define SPLIT_RENDER_VTABLE_RVA 0x3fa678u
#define SPLIT_RENDER_VTABLE_SLOT 0x18u

enum SplitObjectKind {
    SPLIT_UNKNOWN, SPLIT_LOCAL_PLAYER, SPLIT_REMOTE_PLAYER, SPLIT_WEAPON_MANAGER,
    SPLIT_WORLD, SPLIT_INGAME, SPLIT_INPUT_MANAGER, SPLIT_NETWORK, SPLIT_CAMERA,
    SPLIT_OBJECT_KINDS
};
typedef struct {
    int connected;
    uint64_t buttons;
    int lx, ly, rx, ry;
} SplitPad;
typedef struct {
    enum SplitObjectKind kind;
    uint32_t address, vtable, identity, entity, transform, player, camera, weapon;
    uint32_t player_entity, camera_transform;
} SplitObject;

int split_probe_identify(const GuestMem *mem, uint32_t image_base,
                         uint32_t address, uint32_t allocation_size, SplitObject *out);
const char *split_probe_kind_name(enum SplitObjectKind kind);
void split_probe_init(int enabled);
int split_probe_enabled(void);
void split_probe_set_pad(unsigned player, const SplitPad *pad);
const SplitPad *split_probe_pad(unsigned player);
void split_probe_render(uint32_t state);
void split_probe_call_render(GuestCpu *frame, uint32_t original);
void split_probe_camera(uint32_t manager, uint32_t camera, uint32_t weapon);
void split_probe_write(FILE *out, uint32_t live_bytes, uint32_t peak_bytes,
                       uint32_t heap_high_water, uint32_t heap_capacity);
#ifdef __cplusplus
}
#endif
#endif
