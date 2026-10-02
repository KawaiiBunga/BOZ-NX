"""Run the real game zoom updater/setter through the production hook bridge
and production camera_controller.c. Owner's image remains outside Git."""
from pathlib import Path
import math
import struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import *

BASE = 0x800000
DEVICE, CAMERA, WEAPON = 0x302000, 0x303000, 0x305000
STACK, STOP = 0x31F000, 0x13FF00
game = Path('test-artifacts/game-image.bin').read_bytes()
fixture = Path('test-artifacts/camera.bin').read_bytes()
symbols = {}
for line in Path('test-artifacts/camera.nm').read_text().splitlines():
    parts = line.split()
    if len(parts) == 3:
        symbols[parts[2]] = int(parts[0], 16)
assert game[0x21f640:0x21f648] == bytes.fromhex('2de9f04104466d4d')


def word(offset):
    return struct.unpack_from('<I', game, offset)[0]


def float_bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def setup(patch=True):
    u = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    u.ctl_set_cpu_model(UC_CPU_ARM_MAX)  # Production AArch32 compiler can emit ARMv8 VSEL.
    u.mem_map(BASE, 0x600000); u.mem_write(BASE, game)
    u.mem_map(0x100000, 0x60000); u.mem_write(0x100000, fixture)
    u.mem_map(0x300000, 0x20000)
    u.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
    def put(address, value):
        u.mem_write(address, struct.pack('<I', value))
    # Exact production trampoline relocation: PUSH.W/MOV, island-local LDR,
    # then resume original code with its original PC-relative addressing.
    trampoline = game[0x21f640:0x21f646] + struct.pack('<H', 0x4D02)
    trampoline += struct.pack('<III', 0xF000F8DF, BASE + 0x21F649, word(0x21F7FC))
    u.mem_write(0x130100, trampoline)
    if patch:
        u.mem_write(BASE + 0x21F640, struct.pack('<II', 0xF000F8DF,
                    symbols['native_hook_veneers'] + 8*12))
    # Initialize the existing game's cached config/clock/context references.
    config = BASE + 0x21F64E + word(0x21F7FC)
    for guard in (0x74, 0x7C, 0x84, 0x8C):
        put(config + guard, 1)
    for slot, obj in ((0x78,0x304000),(0x80,0x304100),(0x88,0x304200),(0x90,0x304300)):
        put(config + slot, obj)
    put(0x304000+0x10, 0)  # EnableSetFov off: normal ADS transition.
    put(0x304100+0x10, float_bits(80))
    put(0x304200+0x10, float_bits(160))
    put(0x304300+0x10, float_bits(160))
    clock_got = BASE + 0x21F650 + word(0x21F800) + word(0x21F834)
    put(clock_got, 0x306000); put(0x306000+0x38, float_bits(1/60))
    gx_got = BASE + 0xB95E4 + word(0xB9614) + word(0xB9618)
    put(gx_got, 0x306100); put(0x306100, 0x307000)
    put(0x307000+0x344, 1280)  # Engine perspective reference extent.
    put(DEVICE+0x78, CAMERA)
    put(DEVICE+0xC0, float_bits(40)); put(DEVICE+0xC4, float_bits(40))
    put(DEVICE+0x1E0, WEAPON); put(WEAPON+0x278, float_bits(20))
    put(CAMERA+0x40, float_bits(40))
    put(CAMERA+0x44, int(1280/(2*math.tan(math.radians(40)))))
    functions = {BASE+0x35CF04: math.tan, symbols['tanf']:math.tan, symbols['atanf']:math.atan}
    def math_call(machine, address, size, user):
        if address not in functions:
            return
        value = struct.unpack('<f', struct.pack('<I', machine.reg_read(UC_ARM_REG_R0)))[0]
        machine.reg_write(UC_ARM_REG_R0, float_bits(functions[address](value)))
        machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))
    u.hook_add(UC_HOOK_CODE, math_call)
    return u


def put(u, address, value):
    u.mem_write(address, struct.pack('<I', value))


def get(u, address, floating=False):
    return struct.unpack('<f' if floating else '<I', u.mem_read(address, 4))[0]


def tick(u, degrees=0, ads=False):
    put(u, symbols['camera_state'], degrees)
    u.mem_write(DEVICE+0xBC, bytes([int(ads)]))
    u.reg_write(UC_ARM_REG_SP, STACK)
    u.reg_write(UC_ARM_REG_LR, STOP | 1)
    u.reg_write(UC_ARM_REG_R0, DEVICE)
    saved = {}
    for register in (UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,
                     UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11):
        saved[register] = 0xAABB0000 + register
        u.reg_write(register, saved[register])
    try:
        u.emu_start(BASE+0x21F641, STOP, count=100000)
    except Exception:
        pc=u.reg_read(UC_ARM_REG_PC)
        print(f'Camera execution failed at {pc:08x}, bytes {bytes(u.mem_read(pc,8)).hex()}')
        raise
    assert u.reg_read(UC_ARM_REG_PC) == STOP
    assert u.reg_read(UC_ARM_REG_SP) == STACK
    for register, value in saved.items():
        assert u.reg_read(register) == value
    return get(u,DEVICE+0xC0,True),get(u,CAMERA+0x40,True),get(u,CAMERA+0x44)


plain, hooked = setup(False), setup()
for ads in [False]*2 + [True]*35 + [False]*20:
    assert tick(plain,0,ads) == tick(hooked,0,ads), 'Original lens/zoom changed at default'
assert get(hooked,symbols['setter_calls']) == 0

for degrees in (60,90,110):
    u = setup()
    current,lens,focal = tick(u,degrees)
    assert current==40 and lens==degrees/2
    expected=int(1280/(2*math.tan(math.radians(degrees/2))))
    assert abs(focal-expected)<=1
    assert get(u,DEVICE+0xC4,True)==40
    for ads in [True]*35 + [False]*20:
        current,lens,focal = tick(u,degrees,ads)
        ratio=math.tan(math.radians(current))/math.tan(math.radians(40))
        actual=math.tan(math.radians(lens))/math.tan(math.radians(degrees/2))
        assert abs(actual-ratio)<2e-6
        assert abs(focal-int(1280/(2*math.tan(math.radians(lens)))))<=1
    current,lens,focal = tick(u,0)
    assert current==40 and lens==40 and get(u,symbols['camera_state']+8)==0
    before=get(u,symbols['setter_calls'])
    tick(u,0); assert get(u,symbols['setter_calls'])==before

print('PASS: real game camera update/setter + production controller/hook; default identical, 60/90/110 FOV, ADS transitions, focal multiplier, reset and callee-save registers')
