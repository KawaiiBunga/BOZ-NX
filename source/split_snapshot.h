#ifndef BOZ_SPLIT_SNAPSHOT_H
#define BOZ_SPLIT_SNAPSHOT_H
#include <stdint.h>
#include <stdio.h>

/* Private, manual development fixture. Contains the owner's game bytes.
 * Called between game callbacks on the owning thread, never every frame. */
int split_snapshot_write(FILE *out, const char *build, uint32_t frame,
                         uint32_t image_base, const void *image, uint32_t image_bytes,
                         uint32_t heap_base, const void *heap, uint32_t heap_used,
                         uint32_t heap_capacity);
#endif
