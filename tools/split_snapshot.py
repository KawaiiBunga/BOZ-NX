#!/usr/bin/env python3
"""Validate a private actor snapshot. Does not run or modify the game."""
import argparse
from dataclasses import dataclass
from pathlib import Path
import struct

IMAGE_LIMIT = 16 * 1024 * 1024
HEAP_LIMIT = 320 * 1024 * 1024
HEADER_BYTES = 80
MAX_FILE_BYTES = HEADER_BYTES + IMAGE_LIMIT + HEAP_LIMIT + 12


@dataclass(frozen=True)
class Snapshot:
    build: str
    frame: int
    image_base: int
    image: bytes
    heap_base: int
    heap: bytes
    heap_capacity: int

    def read(self, address, size):
        if size < 0:
            raise ValueError('Negative memory span')
        for base, data in ((self.image_base, self.image), (self.heap_base, self.heap)):
            offset = address - base
            if 0 <= offset <= len(data) and size <= len(data) - offset:
                return data[offset:offset + size]
        raise ValueError(f'Uncaptured memory span {address:08x}+{size:x}')

    def word(self, address):
        return struct.unpack('<I', self.read(address, 4))[0]


def parse(data):
    if len(data) < HEADER_BYTES + 12 or len(data) > MAX_FILE_BYTES:
        raise ValueError('Invalid snapshot size')
    if data[:8] != b'BOZSNAP1':
        raise ValueError('Invalid snapshot magic')
    version, header, image_base, image_bytes, heap_base, heap_used, capacity, frame = \
        struct.unpack_from('<8I', data, 8)
    if version != 1 or header != HEADER_BYTES or data[72:80] != bytes(8):
        raise ValueError('Unsupported snapshot header')
    if not (0 < image_bytes <= IMAGE_LIMIT and 0 < capacity <= HEAP_LIMIT and heap_used <= capacity):
        raise ValueError('Invalid image/heap bounds')
    if not image_base or not heap_base or image_base + image_bytes > 0xffffffff or heap_base + capacity > 0xffffffff:
        raise ValueError('Invalid memory addresses')
    if image_base < heap_base + capacity and heap_base < image_base + image_bytes:
        raise ValueError('Overlapping memory ranges')
    if len(data) != HEADER_BYTES + image_bytes + heap_used + 12:
        raise ValueError('Incomplete snapshot payload')
    label = data[40:72]
    if b'\0' not in label:
        raise ValueError('Unterminated build label')
    try:
        build = label.split(b'\0', 1)[0].decode('ascii')
    except UnicodeDecodeError as error:
        raise ValueError('Invalid build label') from error
    if data[-12:-4] != b'BOZEND1\0':
        raise ValueError('Missing completion footer')
    # FNV-1a detects truncated/corrupted development captures, not authenticity.
    value = 2166136261
    for byte in memoryview(data)[:-12]:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    if value != struct.unpack_from('<I', data, len(data) - 4)[0]:
        raise ValueError('Snapshot checksum mismatch')
    return Snapshot(build, frame, image_base, data[header:header + image_bytes],
                    heap_base, data[header + image_bytes:-12], capacity)


def load(path):
    path = Path(path)
    if path.stat().st_size > MAX_FILE_BYTES:
        raise ValueError('Snapshot exceeds supported size')
    return parse(path.read_bytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot', type=Path)
    args = parser.parse_args()
    try:
        snapshot = load(args.snapshot)
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')
    print(f'Build {snapshot.build}, frame {snapshot.frame}')
    print(f'Image {snapshot.image_base:08x}: {len(snapshot.image):,} bytes')
    print(f'Heap {snapshot.heap_base:08x}: {len(snapshot.heap)/1048576:.1f} MiB of {snapshot.heap_capacity/1048576:.0f} MiB')
    print('Validated private game-memory fixture; no second actor or co-op was executed.')


if __name__ == '__main__':
    main()
