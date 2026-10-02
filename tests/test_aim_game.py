"""Exercise the real game pad calculation and production native hook bridge.

Requires an ignored game-image.bin produced by tests/load_image with the
owner's image. No game bytes are stored in the test source.
"""
from pathlib import Path
import struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import *

base = 0x800000
game = Path('test-artifacts/game-image.bin').read_bytes()
bridge = Path('test-artifacts/bridge.bin').read_bytes()
symbols = {}
for line in Path('test-artifacts/bridge.nm').read_text().splitlines():
    parts = line.split()
    if len(parts) == 3:
        symbols[parts[2]] = int(parts[0], 16)
signature = bytes.fromhex('c6ed107abde8f88f')
assert game[0x11fb3c:0x11fb44] == signature


def calculate(x, y, percent_x, percent_y, left=False, patch=True):
    u = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    u.mem_map(base, 0x600000); u.mem_write(base, game)
    u.mem_map(0x100000, 0x40000); u.mem_write(0x100000, bridge)
    u.mem_map(0x300000, 0x20000)
    u.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
    stop = 0x130000
    this = 0x302000; stack = 0x310000
    for offset, value in ((0x4c, 0.2), (0x50, 0.5), (0x54, 0.8), (0x58, 0.5)):
        u.mem_write(this + offset, struct.pack('<f', value))
    ix, iy, w, h, ox, oy = (0x303000 + 4*i for i in range(6))
    for address, value in ((ix, x), (iy, y), (w, 960), (h, 544)):
        u.mem_write(address, struct.pack('<i', value))
    u.mem_write(stack, struct.pack('<4I', w, h, ox, oy))
    for register, value in ((UC_ARM_REG_R0, this), (UC_ARM_REG_R1, int(left)),
                            (UC_ARM_REG_R2, ix), (UC_ARM_REG_R3, iy),
                            (UC_ARM_REG_SP, stack), (UC_ARM_REG_LR, stop | 1)):
        u.reg_write(register, value)
    island = base + 0x500000
    if patch:
        u.mem_write(base + 0x11fb3c, struct.pack('<2I', 0xf000f8df, symbols['native_hook_veneers'] + 7*12))
        u.mem_write(island, signature + struct.pack('<2I', 0xf000f8df, base + 0x11fb45))

    def visit(uc, address, size, unused):
        if address == base + 0x210fc0:  # unrelated UI/device mode query
            uc.reg_write(UC_ARM_REG_R0, 0)
            uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
        elif address == symbols['native_dispatch']:
            assert uc.reg_read(UC_ARM_REG_R1) == 519
            frame = uc.reg_read(UC_ARM_REG_R0)
            owner = struct.unpack('<I', uc.mem_read(frame + 6*4, 4))[0]
            yaw = struct.unpack('<f', uc.mem_read(owner + 0x3c, 4))[0]
            pitch = struct.unpack('<f', uc.mem_read(frame + 68 + 15*4, 4))[0]
            uc.mem_write(owner + 0x3c, struct.pack('<f', yaw * percent_x / 100))
            uc.mem_write(frame + 68 + 15*4, struct.pack('<f', pitch * percent_y / 100))
            uc.mem_write(frame + 15*4, struct.pack('<I', island | 1))

    u.hook_add(UC_HOOK_CODE, visit)
    u.emu_start((base + 0x11f964) | 1, stop, count=2000)
    assert u.reg_read(UC_ARM_REG_PC) == stop
    assert u.reg_read(UC_ARM_REG_SP) == stack
    return struct.unpack('<2f', u.mem_read(this + (0x34 if left else 0x3c), 8))


for x in (648, 768, 888):
    for y in (152, 272, 392):
        baseline = calculate(x, y, 100, 100, patch=False)
        unchanged = calculate(x, y, 100, 100)
        assert baseline == unchanged
        high = calculate(x, y, 250, 225)
        for actual, value, percent in zip(high, baseline, (250, 225)):
            assert abs(actual - value * percent / 100) < 1e-6
left_base = calculate(192, 272, 100, 100, left=True, patch=False)
assert calculate(192, 272, 250, 250, left=True) == left_base
print('PASS: real game pad calculation plus native hook: 100% unchanged, axes scale independently beyond the clamp, left pad unchanged')
