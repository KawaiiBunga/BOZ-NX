/* BOZ's ARM/Thumb code executes directly. This is an ABI bridge, not a CPU
 * emulator. The existing SDK handlers see a small saved register frame.
 * All executable patches are made in the staging phase before RX sealing. */
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "native_exec.h"
#include "selfproc.h"
#include "code_flush.h"
#include "split_probe.h"

_Static_assert(sizeof(void *) == 4, "BOZ must be built for AArch32");
_Static_assert(offsetof(GuestCpu, cpsr) == 64, "assembly frame");
_Static_assert(offsetof(GuestCpu, s) == 68, "assembly VFP frame");
_Static_assert(offsetof(GuestCpu, fpscr) == 324, "assembly FPSCR frame");
_Static_assert(sizeof(GuestCpu) == 332, "assembly frame size");

/* Verified against the imported image hash at boot. Default SDK functions
 * reach RVA 0x3bbfc0; writable tables start later. Validate on hardware. */
#define IMAGE_BASE 0x00800000u
#define TEXT_END   0x003c0000u
extern const unsigned char native_veneers[];
extern const unsigned char native_hook_veneers[];
extern void native_call_entry(uint32_t fn, uint32_t stack_top);
extern void native_leave(void);
static Guest *game;
static S3eImage *image;
static void *backing, *temporary;
static VirtmemReservation *reservation, *tmp_reservation;
static uint32_t mapping_size, hook_resume[10];
static uint64_t imports;
static int running;

uint32_t native_image_base(void) { return IMAGE_BASE; }
uint32_t native_stub_address(unsigned i) {
  return i < 512 ? (uint32_t)(uintptr_t)(native_veneers + i * 12) : 0;
}
int native_reserve_image(uint32_t bytes) {
  mapping_size = (bytes + 0xfffu) & ~0xfffu;
  MemoryInfo mi; u32 pi;
  if (R_FAILED(svcQueryMemory(&mi, &pi, IMAGE_BASE)) || mi.type != MemType_Unmapped ||
      mi.addr > IMAGE_BASE || mi.addr + mi.size < (uint64_t)IMAGE_BASE + mapping_size) {
    printf("[native] image address 0x%x unavailable\n", IMAGE_BASE); return 0;
  }
  virtmemLock();
  reservation = virtmemAddReservation((void *)IMAGE_BASE, mapping_size);
  virtmemUnlock();
  return reservation != NULL;
}
static int failed(const char *step, Result rc) {
  if (R_SUCCEEDED(rc)) return 0;
  printf("[native] %s failed: %08x\n", step, rc); return 1;
}
int native_stage_image(S3eImage *img) {
  if (mapping_size != img->image_alloc) return 0;
  backing = img->image;
  virtmemLock();
  temporary = virtmemFindCodeMemory(mapping_size, 0x1000);
  tmp_reservation = temporary ? virtmemAddReservation(temporary, mapping_size) : NULL;
  virtmemUnlock();
  if (!tmp_reservation) return 0;
  Handle self = dcr_self_process();
  if (failed("staging code mapping", svcMapProcessCodeMemory(self, (uintptr_t)temporary,
             (uintptr_t)backing, mapping_size))) return 0;
  if (failed("writable staging alias", svcMapProcessMemory((void *)IMAGE_BASE, self,
             (uintptr_t)temporary, mapping_size))) return 0;
  img->image = (void *)IMAGE_BASE;
  return 1;
}
static void put32(void *p, uint32_t w) { memcpy(p, &w, 4); }

/* Displaced instructions are verified; PC-relative loads get relocated pools.
 * The two mid-function
 * hooks retain every VFP register and CPSR; imported ABI calls retain the core
 * frame and rely on ordinary AAPCS VFP callee-save rules. */
int native_prepare(Guest *g, S3eImage *img) {
  static const struct { uint32_t rva; unsigned char bytes[8]; int thumb; } expected[10] = {
    {0x34c1a8, {0x10,0x40,0x2d,0xe9,0x00,0x40,0xa0,0xe1}, 0},
    {0x34c1e0, {0x38,0x40,0x2d,0xe9,0x00,0x40,0xa0,0xe1}, 0},
    {0x34c1c4, {0x10,0x40,0x2d,0xe9,0x00,0x40,0xa0,0xe1}, 0},
    {0x2710a0, {0x00,0x30,0x50,0xe2,0xf2,0xff,0xff,0x0a}, 0},
    {0x118be8, {0xd4,0xed,0x16,0x7a,0xf4,0xee,0xc7,0x7a}, 1},
    {0x365dc0, {0x00,0xc0,0xa0,0xe1,0x40,0x00,0x52,0xe3}, 0},
    {0x3664f4, {0x03,0x00,0x10,0xe3,0x70,0x00,0x2d,0xe9}, 0},
    {0x11fb3c, {0xc6,0xed,0x10,0x7a,0xbd,0xe8,0xf8,0x8f}, 1},
    {0x21f640, {0x2d,0xe9,0xf0,0x41,0x04,0x46,0x6d,0x4d}, 1},
    {SPLIT_RENDER_RVA, {0x2d,0xe9,0xf0,0x47,0x86,0xb0,0x82,0x4c}, 1},
  };
  game = g; image = img;
  if ((g->hook_count != 9 && g->hook_count != 10) || img->image_size < TEXT_END) return 0;
  for (unsigned i=0; i<g->hook_count; ++i) {
    if ((g->hook[i].addr & ~1u) != IMAGE_BASE + expected[i].rva ||
        memcmp(img->image + expected[i].rva, expected[i].bytes, 8)) {
      printf("[native] hook %u signature mismatch\n", i); return 0;
    }
  }
  unsigned tail = (img->image_size + 0xfffu) & ~0xfffu;
  /* Camera-update entry includes a PC-relative LDR. Its displaced copy loads
   * the original literal value from a new island-local pool. */
  uint32_t camera_literal;
  memcpy(&camera_literal, img->image + 0x21f7fc, 4);
  if (camera_literal != 0x00250e7eu) {
    printf("[native] camera literal signature mismatch\n"); return 0;
  }
  if (g->hook_count == 10) {
    uint32_t literal, render_method;
    memcpy(&literal, img->image + SPLIT_RENDER_LITERAL_RVA, 4);
    memcpy(&render_method, img->image + SPLIT_RENDER_VTABLE_RVA + SPLIT_RENDER_VTABLE_SLOT, 4);
    if (literal != SPLIT_RENDER_LITERAL || render_method != IMAGE_BASE + SPLIT_RENDER_RVA + 1) {
      printf("[native] in-game render literal/vtable mismatch\n"); return 0;
    }
  }
  for (unsigned i=0; i<g->hook_count; ++i) {
    unsigned char *p = img->image + expected[i].rva;
    uint32_t bridge = (uint32_t)(uintptr_t)(native_hook_veneers + 12*i);
    if (i == 8 || i == 9) {
      unsigned char *island = img->image + tail + 32*i;
      memcpy(island, expected[i].bytes, 6); /* PUSH.W plus MOV/SUB SP. */
      uint16_t load = i == 8 ? 0x4d02 : 0x4c02; /* LDR r5/r4,[PC,#8]: island +16. */
      memcpy(island+6, &load, 2);
      put32(island+8, 0xf000f8dfu); put32(island+12, IMAGE_BASE + expected[i].rva + 8 + 1);
      put32(island+16, i == 8 ? camera_literal : SPLIT_RENDER_LITERAL);
      hook_resume[i] = IMAGE_BASE + tail + 32*i + 1;
      put32(p, 0xf000f8dfu); put32(p+4, bridge);
    } else if (!g->hook[i].observe) {
      put32(p, 0xe51ff004u); put32(p+4, bridge); /* ldr pc,[pc,#-4] */
    } else {
      unsigned char *island = img->image + tail + 32*i;
      if (expected[i].thumb) {
        /* Resume whole non-PC-relative instructions. Hook 7's original
         * POP returns to its caller; the appended jump is then unused. */
        memcpy(island, expected[i].bytes, 8);
        put32(island+8, 0xf000f8dfu); /* ldr.w pc,[pc] */
        put32(island+12, IMAGE_BASE + expected[i].rva + 8 + 1);
        hook_resume[i] = IMAGE_BASE + tail + 32*i + 1;
        put32(p, 0xf000f8dfu); put32(p+4, bridge);
      } else {
        /* Displace only SUBS. The following conditional branch stays in place. */
        memcpy(island, expected[i].bytes, 4);
        put32(island+4, 0xe51ff004u); put32(island+8, IMAGE_BASE+expected[i].rva+4);
        hook_resume[i] = IMAGE_BASE + tail + 32*i;
        put32(island+16, 0xe51ff004u); put32(island+20, bridge);
        int32_t delta = (int32_t)((IMAGE_BASE+tail+32*i+16) - (IMAGE_BASE+expected[i].rva+8));
        put32(p, 0xea000000u | (((uint32_t)(delta / 4)) & 0x00ffffffu));
      }
    }
  }
  Handle self = dcr_self_process();
  if (failed("unmap writable alias", svcUnmapProcessMemory((void *)IMAGE_BASE, self,
      (uintptr_t)temporary, mapping_size))) return 0;
  if (failed("end staging mapping", svcUnmapProcessCodeMemory(self, (uintptr_t)temporary,
      (uintptr_t)backing, mapping_size))) return 0;
  virtmemLock(); virtmemRemoveReservation(tmp_reservation); virtmemUnlock();
  if (failed("final code mapping", svcMapProcessCodeMemory(self, IMAGE_BASE,
      (uintptr_t)backing, mapping_size))) return 0;
  if (failed("seal ARM text", svcSetProcessMemoryPermission(self, IMAGE_BASE, TEXT_END, Perm_Rx)) ||
      failed("map game data", svcSetProcessMemoryPermission(self, IMAGE_BASE+TEXT_END,
             tail-TEXT_END, Perm_Rw)) ||
      failed("seal hook islands", svcSetProcessMemoryPermission(self, IMAGE_BASE+tail,
             mapping_size-tail, Perm_Rx))) return 0;
  dcr_code_flush((void *)IMAGE_BASE, TEXT_END);
  dcr_code_flush((void *)(IMAGE_BASE+tail), mapping_size-tail);
  printf("[native] direct ARM32: text=%x data=%x, %u imports, %u validated hooks\n",
         TEXT_END, tail-TEXT_END, g->hle.count, g->hook_count);
  return 1;
}
uint32_t native_original_hook(unsigned hook) {
  return game && hook < game->hook_count ? hook_resume[hook] : 0;
}
void native_dispatch(GuestCpu *f, unsigned slot) {
  if (slot < 512) {
    if (slot >= game->hle.count || !game->hle.slot[slot].fn) {
      printf("[native] unbound import %u\n", slot); native_request_exit(); return;
    }
    GuestHleSlot *s = &game->hle.slot[slot];
    ++imports;
    f->r[15] = f->r[14];
    if (game->prof) game->prof(slot,1);
    s->fn(f, &game->mem, s->user ? s->user : game->hle.user);
    if (game->prof) game->prof(slot,0);
  } else {
    unsigned h = slot - 512;
    if (h >= game->hook_count) { native_request_exit(); return; }
    GuestHook *k = &game->hook[h];
    f->r[15] = k->addr & ~1u;
    uint32_t initial = f->r[15];
    k->fn(f, &game->mem, k->user);
    if (!k->observe) f->r[15] = f->r[14];
    else if (f->r[15] == initial) f->r[15] = hook_resume[h];
    else if (h == 4) f->r[15] |= 1; /* Thumb loop's fast-forward destination */
  }
}
void native_enter(Guest *g) {
  running = 1;
  native_call_entry(g->cpu.r[15], g->cpu.r[13]);
  running = 0;
}
void native_request_exit(void) { if (running) native_leave(); }
GuestStatus guest_call_r0_3(Guest *g, uint32_t fn, uint32_t a, uint32_t b, uint32_t c, uint32_t *ret) {
  (void)g;
  if (!fn) return GUEST_FAULT_MEM;
  uint32_t value = ((uint32_t (*)(uint32_t,uint32_t,uint32_t))(uintptr_t)fn)(a,b,c);
  if (ret) *ret = value;
  return GUEST_OK;
}
GuestStatus guest_call_r0(Guest *g,uint32_t fn,uint32_t a,uint32_t b,uint32_t *ret) {
  return guest_call_r0_3(g,fn,a,b,0,ret);
}
GuestStatus guest_call(Guest *g,uint32_t fn,uint32_t a,uint32_t b) {
  return guest_call_r0_3(g,fn,a,b,0,NULL);
}
void native_counters(uint64_t *i,uint64_t *t,uint64_t *c) { *i=0; *t=0; *c=imports; }
void native_diag(void) {
  printf("[native] image=%08x SDK calls=%llu; direct CPU execution (instruction counts unavailable)\n",
         image ? image->load_base : 0, (unsigned long long)imports);
}
