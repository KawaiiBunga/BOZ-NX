#include "split_snapshot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char image[] = "fixture image 12";
    static unsigned char heap[70001];
    for (unsigned i = 0; i < sizeof heap; ++i) heap[i] = (unsigned char)(i * 7 + 3);
    FILE *out = fopen("test-artifacts/snapshot-fixture.bin", "wb");
    assert(out);
    assert(split_snapshot_write(out, "codboz-test", 17, 0x800000, image, sizeof image,
                                0x40000000, heap, sizeof heap, 1048576));
    assert(!fclose(out));
    out = fopen("test-artifacts/snapshot-empty.bin", "wb");
    assert(out);
    assert(split_snapshot_write(out, "codboz-test", 0, 0x800000, image, sizeof image,
                                0x40000000, heap, 0, 1048576));
    assert(!fclose(out));
    out = tmpfile();
    assert(out);
    assert(!split_snapshot_write(out, "test", 0, 0x800000, image, sizeof image,
                                 0x40000000, heap, 1048577, 1048576));
    assert(!split_snapshot_write(out, "test", 0, 0xfffffff8, image, sizeof image,
                                 0x40000000, heap, 1, 1048576));
    assert(!split_snapshot_write(out, "test", 0, 0x800000, image, sizeof image,
                                 0x800008, heap, 1, 1048576));
    assert(!split_snapshot_write(out, "label too long for this fixture format", 0,
                                 0x800000, image, sizeof image, 0x40000000, heap, 1, 1048576));
    assert(ftell(out) == 0);
    assert(!fclose(out));
    out = fopen("/dev/full", "wb");
    assert(out);
    setvbuf(out, NULL, _IONBF, 0);
    assert(!split_snapshot_write(out, "test", 0, 0x800000, image, sizeof image,
                                 0x40000000, heap, sizeof heap, 1048576));
    fclose(out);
    puts("PASS: actor snapshot writer bounds, chunked payload, empty heap and write failure");
    return 0;
}
