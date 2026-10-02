#include "split_snapshot.h"
#include <string.h>

#define IMAGE_LIMIT (16u * 1024u * 1024u)
#define HEAP_LIMIT (320u * 1024u * 1024u)
#define HEADER_BYTES 80u

static void put32(unsigned char *p, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (unsigned char)(value >> (i * 8));
}

static int write_span(FILE *out, const void *data, uint32_t bytes, uint32_t *hash) {
    const unsigned char *p = data;
    while (bytes) {
        uint32_t count = bytes > 65536 ? 65536 : bytes;
        if (fwrite(p, 1, count, out) != count) return 0;
        for (uint32_t i = 0; i < count; ++i) *hash = (*hash ^ p[i]) * 16777619u;
        p += count;
        bytes -= count;
    }
    return 1;
}

int split_snapshot_write(FILE *out, const char *build, uint32_t frame,
                         uint32_t image_base, const void *image, uint32_t image_bytes,
                         uint32_t heap_base, const void *heap, uint32_t heap_used,
                         uint32_t heap_capacity) {
    if (!out || !build || !image || !heap || strlen(build) >= 32 ||
        !image_bytes || image_bytes > IMAGE_LIMIT || !heap_capacity ||
        heap_capacity > HEAP_LIMIT || heap_used > heap_capacity ||
        !image_base || !heap_base || image_base > UINT32_MAX - image_bytes ||
        heap_base > UINT32_MAX - heap_capacity ||
        (image_base < heap_base + heap_capacity && heap_base < image_base + image_bytes))
        return 0;
    unsigned char header[HEADER_BYTES] = {0}, footer[12] = {0};
    memcpy(header, "BOZSNAP1", 8);
    put32(header + 8, 1);
    put32(header + 12, HEADER_BYTES);
    put32(header + 16, image_base);
    put32(header + 20, image_bytes);
    put32(header + 24, heap_base);
    put32(header + 28, heap_used);
    put32(header + 32, heap_capacity);
    put32(header + 36, frame);
    memcpy(header + 40, build, strlen(build));
    uint32_t hash = 2166136261u;
    if (!write_span(out, header, HEADER_BYTES, &hash) ||
        !write_span(out, image, image_bytes, &hash) ||
        !write_span(out, heap, heap_used, &hash)) return 0;
    memcpy(footer, "BOZEND1", 7);
    put32(footer + 8, hash);
    return fwrite(footer, 1, sizeof footer, out) == sizeof footer && !ferror(out);
}
