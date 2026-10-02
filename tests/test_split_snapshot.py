"""Cross-language snapshot fixture and damaged/truncated input checks."""
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.split_snapshot import load, parse

path = Path('test-artifacts/snapshot-fixture.bin')
data = path.read_bytes()
snapshot = load(path)
assert snapshot.build == 'codboz-test' and snapshot.frame == 17
assert snapshot.image == b'fixture image 12\0'
assert snapshot.heap == bytes((i * 7 + 3) & 255 for i in range(70001))
assert snapshot.heap_capacity == 1048576
assert snapshot.word(snapshot.image_base) == int.from_bytes(b'fixt', 'little')
assert snapshot.read(snapshot.heap_base + 65530, 24) == snapshot.heap[65530:65554]
assert snapshot.read(snapshot.heap_base + len(snapshot.heap), 0) == b''
assert load('test-artifacts/snapshot-empty.bin').heap == b''
for address, size in ((0, 1), (snapshot.heap_base - 1, 4),
                      (snapshot.heap_base + len(snapshot.heap), 1),
                      (snapshot.image_base + len(snapshot.image) - 1, 2),
                      (snapshot.heap_base, -1)):
    try:
        snapshot.read(address, size)
    except ValueError:
        pass
    else:
        raise AssertionError('Uncaptured memory accepted')
damaged = bytearray(data)
damaged[80 + len(snapshot.image) + 65536] ^= 1
bad_bounds = bytearray(data)
struct.pack_into('<I', bad_bounds, 28, snapshot.heap_capacity + 1)
bad_address = bytearray(data)
struct.pack_into('<I', bad_address, 24, 0xfffffffe)
bad_label = bytearray(data)
bad_label[40:72] = b'a' * 32
for invalid in (b'', data[:80], data[:-1], data + b'\0', b'!' + data[1:],
                data[:-12] + b'!' + data[-11:], damaged, bad_bounds, bad_address, bad_label):
    try:
        parse(invalid)
    except ValueError:
        pass
    else:
        raise AssertionError('Invalid fixture accepted')
print('PASS: C actor fixture parsed on PC; exact spans/checksum and corrupt/truncated input rejection')
