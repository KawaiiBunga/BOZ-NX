"""Clone the real local-player prefab on a PC copy of an owner's capture.

Game constructors, component property copies and post-copy methods execute
their original instructions. Allocator/free/random SDK services are modeled.
This deliberately stops before gameplay initialization, physics, input, GPU
rendering and co-op. It never modifies the snapshot, Switch or game files.
Run tools/check.ps1 with -GameImage and -Snapshot to generate the input manifest.
"""
import argparse
from collections import Counter, deque
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.split_snapshot import load
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import *

BASE, TEXT_END = 0x800000, 0x3c0000
STACK, STOP = 0x31f000, 0x31ff00
ARENA, ARENA_END = 0x400000, 0x700000
PATCHES = (0x34c1a8, 0x34c1e0, 0x34c1c4, 0x2710a0, 0x118be8,
           0x365dc0, 0x3664f4, 0x11fb3c, 0x21f640, 0x193060)


class Sandbox:
    def __init__(self, snapshot, image_path, imports_path):
        pristine = Path(image_path).read_bytes()
        assert snapshot.image_base == BASE and len(pristine) >= len(snapshot.image)
        assert len(snapshot.image) >= TEXT_END
        captured_text = bytearray(snapshot.image[:TEXT_END])
        for offset in PATCHES:
            captured_text[offset:offset + 8] = pristine[offset:offset + 8]
        # The port also redirects Player-%d's PC-relative format literal to
        # its configured name in the game heap (source/main.c boot patch).
        assert pristine[0x3af131:0x3af13b] == b'Player-%d\0'
        name_address = (BASE + 0x18f60a + snapshot.word(BASE + 0x18f74c)) & 0xffffffff
        assert snapshot.heap_base <= name_address < snapshot.heap_base + len(snapshot.heap)
        captured_text[0x18f74c:0x18f750] = pristine[0x18f74c:0x18f750]
        assert captured_text == pristine[:TEXT_END], 'Capture/image executable code mismatch'
        self.u = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.u.ctl_set_cpu_model(UC_CPU_ARM_MAX)
        self.u.mem_map(BASE, (len(snapshot.image) + 4095) & ~4095)
        self.u.mem_write(BASE, snapshot.image)
        self.u.mem_write(BASE, bytes(captured_text))
        self.u.mem_map(snapshot.heap_base, (len(snapshot.heap) + 4095) & ~4095)
        self.u.mem_write(snapshot.heap_base, snapshot.heap)
        self.u.mem_map(0x300000, 0x20000)
        self.u.mem_map(ARENA, ARENA_END - ARENA)
        self.u.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
        self.trace = deque(maxlen=16)
        self.hooks, self.sizes, self.live = {}, {}, set()
        self.next_allocation = ARENA
        self.captured_frees = []
        self.sdk_pages = set()
        self.imports = {name: self.word(BASE + int(rva, 16)) for rva, name in
                        (line.split() for line in Path(imports_path).read_text().splitlines())}
        self.import_names = {address & ~1: name for name, address in self.imports.items()}
        for offset in (0x16772, 0x348328, 0x348320, 0x34c1a8):
            self.hooks[BASE + offset] = self.allocate
        for offset in (0x34a4c0, 0x348324, 0x34831c, 0x34c1c4):
            self.hooks[BASE + offset] = self.free
        self.sdk('s3eMallocBase', self.allocate)
        self.sdk('s3eFreeBase', self.free)
        self.sdk('s3eReallocBase', self.reallocate)
        random_values = iter(range(0x310, 0x410))
        self.hooks[BASE + 0x3680f4] = lambda: next(random_values)
        self.u.hook_add(UC_HOOK_CODE, self.intercept)

    def word(self, address):
        return struct.unpack('<I', self.u.mem_read(address, 4))[0]

    def put(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value))

    def pic(self, add_pc, literal):
        return (BASE + add_pc + 4 + self.word(BASE + literal)) & 0xffffffff

    def string(self, address):
        return bytes(self.u.mem_read(address, 256)).split(b'\0')[0].decode('ascii')

    def rtti(self, address):
        try:
            vtable = self.word(address)
            if not BASE + TEXT_END <= vtable < BASE + 0x500000 or self.word(vtable - 8):
                return None
            return self.string(self.word(self.word(vtable - 4) + 4))
        except Exception:
            return None

    def call(self, offset, *arguments, thumb=True):
        saved = {reg: 0xaabb0000 + reg for reg in
                 (UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,
                  UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11)}
        for reg, value in saved.items():
            self.u.reg_write(reg, value)
        self.u.reg_write(UC_ARM_REG_SP, STACK)
        self.u.reg_write(UC_ARM_REG_LR, STOP | 1)
        for index, value in enumerate(arguments):
            if index < 4:
                self.u.reg_write(UC_ARM_REG_R0 + index, value)
            else:
                self.put(STACK + (index - 4) * 4, value)
        try:
            self.u.emu_start(BASE + offset + int(thumb), STOP, count=2000000)
        except Exception as error:
            raise AssertionError(f'{error}; recent PCs: {list(self.trace)}') from error
        assert self.u.reg_read(UC_ARM_REG_PC) == STOP, list(self.trace)
        assert self.u.reg_read(UC_ARM_REG_SP) == STACK
        for reg, value in saved.items():
            assert self.u.reg_read(reg) == value
        return self.u.reg_read(UC_ARM_REG_R0)

    def name_hash(self, name):
        self.u.mem_write(0x310000, name.encode('ascii') + b'\0')
        return self.call(0x24ba2c, 0x310000, thumb=False)

    def sdk(self, name, callback):
        address = self.imports[name]
        # These are native SDK veneers, outside the captured game memory.
        assert address >= BASE + 0x500000
        page = address & ~4095
        if page not in self.sdk_pages:
            self.u.mem_map(page, 4096)
            self.sdk_pages.add(page)
        self.u.mem_write(address, b'\x1e\xff\x2f\xe1')  # ARM BX LR
        self.hooks[address] = callback

    def intercept(self, uc, address, size, user):
        self.trace.append(hex(address))
        if address not in self.hooks:
            if address in self.import_names:
                raise RuntimeError('Unmodeled SDK service: ' + self.import_names[address])
            return
        result = self.hooks[address]()
        if result is not None:
            uc.reg_write(UC_ARM_REG_R0, result)
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))

    def allocate(self):
        size = self.u.reg_read(UC_ARM_REG_R0)
        assert 0 < size < 0x100000
        pointer = self.next_allocation
        self.next_allocation += (size + 15) & ~15
        assert self.next_allocation < ARENA_END
        self.sizes[pointer] = size
        self.live.add(pointer)
        return pointer

    def free(self):
        pointer = self.u.reg_read(UC_ARM_REG_R0)
        if not pointer:
            return
        if pointer in self.sizes:
            assert pointer in self.live, 'Double free in sandbox allocator'
            self.live.remove(pointer)
        else:
            self.captured_frees.append((pointer, self.rtti(pointer), self.u.reg_read(UC_ARM_REG_LR)))
        # Existing allocator metadata isn't captured. Record such frees instead
        # of pretending this restores or frees the original process's memory.

    def reallocate(self):
        old = self.u.reg_read(UC_ARM_REG_R0)
        size = self.u.reg_read(UC_ARM_REG_R1)
        if old:
            assert old in self.live, 'Existing allocation size is not captured'
        if not size:
            self.free()
            return 0
        self.u.reg_write(UC_ARM_REG_R0, size)
        pointer = self.allocate()
        if old:
            self.u.mem_write(pointer, bytes(self.u.mem_read(old, min(size, self.sizes[old]))))
            self.live.remove(old)
        return pointer

    def linked_values(self, head):
        node, seen = self.word(head), set()
        while node != head:
            assert node not in seen and len(seen) < 256
            seen.add(node)
            yield self.word(node + 8)
            node = self.word(node)

    def prefab(self, address, seen=None):
        seen = set() if seen is None else seen
        assert address not in seen and len(seen) < 32
        seen.add(address)
        assert self.rtti(address) == '13CIsEntitySpec'
        components = list(self.linked_values(self.word(address + 0x14)))
        assert all(self.rtti(c) == '16CIsComponentSpec' for c in components)
        return {'address': address, 'hash': self.word(address + 4),
                'components': components,
                'children': [self.prefab(c, seen) for c in self.linked_values(self.word(address + 0x18))]}


def check(sandbox):
    g = sandbox
    prototype = g.call(0x1fa95c, g.name_hash('player_local'), 2)
    assert prototype, 'No loaded local-player resource'
    prefab = g.prefab(prototype)
    assert len(prefab['children']) == 2
    assert [len(prefab['components']), *(len(c['components']) for c in prefab['children'])] == [20, 7, 10]
    original_names = {name: g.call(0xbd514, g.name_hash(name)) for name in ('MainCamera', 'player_weapon')}
    assert all(original_names.values())
    primary_manager = g.call(0x21dd88, original_names['player_weapon'])
    primary_controller = g.word(primary_manager + 0x60)
    primary_camera = g.call(0xb9414, original_names['MainCamera'])
    assert g.rtti(primary_controller) == '17CPlayerController'
    primary_objects = {address: bytes(g.u.mem_read(address, size)) for address, size in
                       ((primary_controller, 816), (primary_manager, 528), (primary_camera, 80))}
    globals_address = g.pic(0xbdb88, 0xbdbe0)
    original_header = g.word(globals_address + 0x24)
    original_count = g.word(globals_address + 0x28)
    private_header = 0x309000
    g.put(private_header + 8, private_header)
    g.put(private_header + 12, private_header)
    factory = g.word(BASE + 0x457920)
    original_entities, original_pending = g.word(factory + 8), g.word(factory + 0x20)
    private_names = {}
    g.put(globals_address + 0x24, private_header)
    g.put(globals_address + 0x28, 0)
    try:
        entity = g.call(0xbb64e, factory, prototype, 2, g.name_hash('boz_split_player_2'))
        assert entity and g.word(entity) == BASE + 0x3e6510
        assert g.word(factory + 8) == original_entities + 3
        # The real recursive child calls use mode 0 even with a mode-2 root.
        assert g.word(factory + 0x20) == original_pending + 2
        assert g.word(globals_address + 0x28) == 3
        for name in ('MainCamera', 'player_weapon', 'boz_split_player_2'):
            private_names[name] = g.call(0xbd514, g.name_hash(name))
            assert private_names[name]
        assert private_names['boz_split_player_2'] == entity
        assert all(private_names[name] != old for name, old in original_names.items())
    finally:
        g.put(globals_address + 0x24, original_header)
        g.put(globals_address + 0x28, original_count)
    assert all(g.call(0xbd514, g.name_hash(name)) == old for name, old in original_names.items())
    changes = [(hex(address), [(hex(offset), data[offset:offset + 4].hex(),
                               bytes(g.u.mem_read(address + offset, 4)).hex())
                              for offset in range(0, len(data), 4)
                              if bytes(g.u.mem_read(address + offset, 4)) != data[offset:offset + 4]])
               for address, data in primary_objects.items()
               if bytes(g.u.mem_read(address, len(data))) != data]
    controller = g.call(0x1d38cc, entity)
    manager = g.call(0x21dd88, private_names['player_weapon'])
    camera = g.call(0xb9414, private_names['MainCamera'])
    assert g.rtti(controller) == '17CPlayerController'
    assert g.rtti(manager) == '14CWeaponManager'
    assert g.rtti(camera) == '9CIsCamera'
    # The real constructor assigns the global in-game weapon reference to its
    # new manager. Name isolation alone cannot preserve player one's ownership.
    # The old manager loses that registered reference; capture this blocker
    # rather than claiming that constructor-only cloning is safe on Switch.
    assert changes == [(hex(primary_manager), [('0x18',
        primary_objects[primary_manager][0x18:0x1c].hex(),
        struct.pack('<I', struct.unpack_from('<I', primary_objects[primary_manager], 0x18)[0] - 1).hex())])], changes
    state_global = g.pic(0x21f9ae, 0x21f9cc)
    state = g.word(state_global)
    assert g.word(state + 0x4c) == manager, 'Expected original constructor global ownership handoff'
    assert all(pointer in g.live for pointer in (controller, manager, camera))
    assert g.word(controller + 0xc) == entity
    assert g.word(manager + 0xc) == private_names['player_weapon']
    assert g.word(camera + 0xc) == private_names['MainCamera']
    assert g.call(0x199714, entity) in g.live  # actual health registry lookup
    # These references need the original initialization path, not just copying.
    assert g.word(controller + 0x168) == 0
    assert g.word(manager + 0x60) == 0 and g.word(manager + 0x78) == 0
    kinds = Counter(g.rtti(a) for a in g.live if g.sizes[a] >= 16)
    assert kinds['17CPlayerController'] == 1 and kinds['14CWeaponManager'] == 1
    assert kinds['9CIsCamera'] == 1 and kinds['7CHealth'] == 1
    print('PASS: real captured prefab creates root/camera/weapon entities and 37 components; '
          'independent name map preserves primary lookup; child initialization remains queued')
    print('PASS: constructor global weapon handoff and primary reference removal reproduced; '
          'name isolation alone is insufficient for a hardware spawn')
    print(f'Construction arena: {g.next_allocation - ARENA:,} bytes across {len(g.sizes)} allocation requests '
          '(SDK fixtures; excludes gameplay/physics/GPU initialization)')
    print('Captured allocations requested for free:',
          [(hex(a), kind, hex(caller)) for a, kind, caller in g.captured_frees])
    print('No gameplay update, initialized player, Switch spawn, second view or co-op was tested.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot', type=Path)
    parser.add_argument('--image', type=Path, default=Path('test-artifacts/game-image.bin'))
    parser.add_argument('--imports', type=Path, default=Path('test-artifacts/game-imports.txt'))
    args = parser.parse_args()
    snapshot = load(args.snapshot)
    print(f'Validated {snapshot.build}, frame {snapshot.frame}; cloning only a PC memory copy', flush=True)
    check(Sandbox(snapshot, args.image, args.imports))


if __name__ == '__main__':
    main()
