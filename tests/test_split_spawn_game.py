"""Exercise the supported game's spawn/clone/name-registration instructions.

The SDK allocator/random source and prototype/component callbacks are fixtures.
No real player prefab is instantiated, and these checks do not prove gameplay,
network ownership or split-screen rendering. The owner's image stays local.
"""
from collections import deque
from pathlib import Path
import struct

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import *

BASE = 0x800000
IMAGE = Path('test-artifacts/game-image.bin').read_bytes()
STOP, STACK = 0x31ff00, 0x31f000


def image_word(offset):
    return struct.unpack_from('<I', IMAGE, offset)[0]


def pic(add_pc, literal):
    return (BASE + add_pc + 4 + image_word(literal)) & 0xffffffff


class Game:
    def __init__(self):
        self.u = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.u.ctl_set_cpu_model(UC_CPU_ARM_MAX)
        self.u.mem_map(BASE, 0x600000)
        self.u.mem_write(BASE, IMAGE)
        self.u.mem_map(0x300000, 0x20000)
        self.u.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
        self.hooks, self.events = {}, []
        self.trace = deque(maxlen=12)
        self.heap = 0x30c000
        self.freed = []
        self.stub(0x16772, self.allocate)
        self.stub(0x348328, self.allocate)
        self.stub(0x34a4c0, lambda g: g.freed.append(g.reg(0)))
        self.u.hook_add(UC_HOOK_CODE, self.intercept)

    def word(self, address):
        return struct.unpack('<I', self.u.mem_read(address, 4))[0]

    def put(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value))

    def reg(self, index):
        return self.u.reg_read(UC_ARM_REG_R0 + index)

    def stub(self, rva, callback):
        self.hooks[BASE + rva] = callback

    def intercept(self, uc, address, size, user):
        self.trace.append(hex(address))
        if address not in self.hooks:
            return
        result = self.hooks[address](self)
        if result is not None:
            uc.reg_write(UC_ARM_REG_R0, result)
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))

    def allocate(self, g):
        size = g.reg(0)
        assert 0 < size <= 0x1000, size
        pointer = self.heap
        self.heap += (size + 15) & ~15
        assert self.heap < 0x31a000
        return pointer

    def tree(self, header):
        self.put(header + 8, header)
        self.put(header + 12, header)

    def call(self, rva, *arguments, thumb=True):
        saved = {register: 0xaabb0000 + register for register in
                 (UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,
                  UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11)}
        for register, value in saved.items():
            self.u.reg_write(register, value)
        self.u.reg_write(UC_ARM_REG_SP, STACK)
        self.u.reg_write(UC_ARM_REG_LR, STOP | 1)
        for index, value in enumerate(arguments):
            if index < 4:
                self.u.reg_write(UC_ARM_REG_R0 + index, value)
            else:
                self.put(STACK + (index - 4) * 4, value)
        try:
            self.u.emu_start(BASE + rva + int(thumb), STOP, count=200000)
        except Exception as error:
            raise AssertionError(f'{error}; recent PCs: {list(self.trace)}') from error
        assert self.u.reg_read(UC_ARM_REG_PC) == STOP, list(self.trace)
        assert self.u.reg_read(UC_ARM_REG_SP) == STACK
        for register, value in saved.items():
            assert self.u.reg_read(register) == value
        return self.reg(0)


def spawn_arguments(named, bind_during_clone, mode, transform_present=True, prototype_present=True):
    g = Game()
    manager, prototype, entity, transform = 0x302000, 0x303000, 0x304000, 0x305000
    position, rotation, name, name_component = 0x306000, 0x306020, 0x306040, 0x306100
    g.put(entity + 4, 0x12345678)
    g.u.mem_write(position, struct.pack('<3f', 1.25, -4.5, 3.0))
    g.u.mem_write(rotation, struct.pack('<4f', 0.0, 0.0, 0.70710677, 0.70710677))
    g.u.mem_write(name, b'Boz_Split_Player_2\0')
    session = 0x307000
    g.put(session + 0xd4, 0x12340000)
    g.put(session + 0xd8, 5)
    # Use the game's real singleton getter and factory-global loads.
    session_slot = pic(0x18c8c8, 0x18c8d0) + image_word(0x18c8d4)
    g.put(g.word(session_slot), session)
    g.put(pic(0x1fa792, 0x1fa814), 0x307100)
    g.stub(0x180f30, lambda game: None)  # ancillary pre-spawn work
    g.stub(0x255e54, lambda game: None)  # SDK diagnostic callback

    def create(game):
        game.events.append(('create', *(game.reg(i) for i in range(4))))
        assert game.reg(0) == 0x307100 and game.reg(1) == prototype
        return entity

    transform_registry = pic(0xc23ec, 0xc23f8) + 8

    def component(game):
        assert game.reg(0) == transform_registry and game.reg(1) == 0x12345678
        return transform if transform_present else 0

    def name_lookup(game):
        assert game.reg(0) == entity
        game.events.append(('name-lookup',))
        return name_component

    g.stub(0xbb64e, create)
    g.stub(0x4eeb0, component)
    g.stub(0xbd4e0, name_lookup)
    # Run the real name setter's field write, with registry/string observers
    # replaced. Registry collision behavior is tested separately below.
    g.stub(0xbd548, lambda game: None)
    g.stub(0x17d5c, lambda game: None)
    g.stub(0xbdb78, lambda game: game.events.append(('name-set', game.word(name_component + 0x2c))))

    result = g.call(0x1fa81c, manager, prototype if prototype_present else 0,
                    position, rotation, name if named else 0, mode, int(bind_during_clone))
    if not prototype_present:
        assert result == 0 and not g.events
    else:
        assert result == entity
        factory_args = g.events[0]
        assert factory_args[3] == mode
        if named:
            expected_hash = g.call(0x24ba2c, name, thumb=False)
            assert g.word(session + 0xd8) == 5 and g.word(manager + 0x1c) == 0
        else:
            expected_hash = 0x12340005
            assert g.word(session + 0xd8) == 6 and g.word(manager + 0x1c) == 1
        assert factory_args[4] == (expected_hash if bind_during_clone else 0)
        if not bind_during_clone:
            assert ('name-set', expected_hash) in g.events
        else:
            assert all(event[0] != 'name-lookup' for event in g.events)
        if transform_present:
            assert bytes(g.u.mem_read(transform + 0xa8, 12)) == bytes(g.u.mem_read(position, 12))
            assert bytes(g.u.mem_read(transform + 0x98, 16)) == bytes(g.u.mem_read(rotation, 16))
            assert bytes(g.u.mem_read(transform + 0x30, 1)) == b'\1'
        else:
            assert bytes(g.u.mem_read(transform + 0x98, 28)) == bytes(28)


def entity_creation(mode, busy=False, missing=False):
    """Real clone control flow, ID generation, entity ctor and tree insertion;
    the prototype copy and component methods use a minimal scene fixture.
    """
    g = Game()
    factory, prototype, tree, pending = 0x302000, 0x303000, 0x304000, 0x304100
    children, prototype_vtable = 0x304200, 0x304300
    registry, types, type_node = 0x305000, 0x305100, 0x305200
    component, component_vtable, component_registry = 0x306000, 0x306100, 0x306200
    g.put(factory + 4, tree); g.tree(tree)
    g.put(factory + 0x1c, pending); g.tree(pending)
    g.put(factory + 0x28, int(busy))
    g.put(prototype + 0x18, children); g.put(children, children); g.put(children + 4, children)
    g.put(prototype, prototype_vtable)
    g.put(prototype_vtable + 0x40, 0x31b001)
    g.u.mem_write(0x31b000, b'\x70\x47')  # prototype-copy callback fixture
    g.hooks[0x31b000] = lambda game: game.events.append(('copy', game.reg(0), game.reg(1)))
    g.put(registry + 0xc, types)
    g.put(types, type_node); g.put(types + 4, type_node)
    g.put(type_node, types); g.put(type_node + 4, types)
    g.put(type_node + 8, component_registry)
    g.put(component, component_vtable)
    g.put(component + 0x10, 0xa7)
    g.put(component_vtable + 0x4c, 0x31b101)
    g.put(component_vtable + 0x58, 0x31b201)
    g.u.mem_write(0x31b100, b'\x70\x47')
    g.u.mem_write(0x31b200, b'\x70\x47')
    g.hooks[0x31b100] = lambda game: game.events.append(('post-copy', game.reg(0)))
    g.hooks[0x31b200] = lambda game: game.events.append(('init', game.reg(0)))
    random_values = iter((1, 2))
    g.stub(0x3680f4, lambda game: next(random_values))
    g.stub(0xba940, lambda game: registry)
    def lookup(game):
        if game.reg(0) == component_registry:
            assert game.reg(1) == 0x20002
            return component
        return 0  # no hierarchy component in this fixture

    g.stub(0x4eeb0, lookup)
    result = g.call(0xbb64e, factory, 0 if missing else prototype, mode, 0)
    if busy or missing:
        assert result == 0 and not g.events and g.heap == 0x30c000
        return
    assert result != 0 and g.word(result) == BASE + 0x3e6510
    assert g.word(result + 4) == 0x20002
    expected_events = [('copy', prototype, result), ('post-copy', component)]
    if mode == 1:
        expected_events.append(('init', component))
    assert g.events == expected_events, g.events
    assert bytes(g.u.mem_read(component + 0x10, 1)) == bytes([0xa5 if mode == 1 else 0xa7])
    assert g.word(factory + 8) == 1
    root = g.word(tree + 4)
    assert g.word(root + 0x10) == 0x20002 and g.word(root + 0x14) == result
    assert g.word(factory + 0x20) == int(mode == 0)


def name_registration():
    """Execute the real global name-map insert, overwrite, lookup and removal.

    Reusing a prefab's names redirects lookup even when the two entity IDs are
    distinct. A later name scope must handle destruction as well as creation.
    """
    g = Game()
    header, first, second, third = 0x302000, 0x303000, 0x304000, 0x305000
    names = pic(0xbdb88, 0xbdbe0)
    assert names == pic(0xbd518, 0xbd534)
    g.put(names + 0x24, header); g.tree(header)
    g.put(names + 0x28, 0)
    common, unique = 0x51aabbcc, 0x51aabbcd
    for component, entity, name_hash in ((first, 0x306000, common),
                                          (second, 0x307000, common),
                                          (third, 0x308000, unique)):
        g.put(component + 0xc, entity)
        g.put(component + 0x2c, name_hash)
    assert g.call(0xbd514, common) == 0
    g.call(0xbdb78, first)
    assert g.call(0xbd514, common) == 0x306000
    assert g.word(names + 0x28) == 1
    allocated = g.heap
    g.call(0xbdb78, second)
    assert g.call(0xbd514, common) == 0x307000
    assert g.word(names + 0x28) == 1 and g.heap == allocated
    g.call(0xbdb78, third)
    assert g.call(0xbd514, unique) == 0x308000
    assert g.call(0xbd514, common) == 0x307000
    assert g.word(names + 0x28) == 2
    # Removing the older component removes the shared name too: the game's
    # unregister path does not check that the map still points to that entity.
    g.call(0xbd548, first)
    assert g.call(0xbd514, common) == 0
    assert g.call(0xbd514, unique) == 0x308000
    assert g.word(names + 0x28) == 1
    g.put(second + 0x2c, 0)
    allocated = g.heap
    g.call(0xbdb78, second)
    assert g.word(names + 0x28) == 1 and g.heap == allocated


for named in (False, True):
    for bind in (False, True):
        for mode in (0, 1, 2):
            spawn_arguments(named, bind, mode)
spawn_arguments(True, False, 0, transform_present=False)
spawn_arguments(False, False, 0, prototype_present=False)
for mode in (0, 1, 2):
    entity_creation(mode)
entity_creation(0, busy=True)
entity_creation(0, missing=True)
name_registration()
print('PASS: actual spawn arguments/name hash/transform; clone guards, ID/ctor/tree, post-copy/init/deferred queue; global name collision/lookup/removal (prototype/SDK/component fixtures; no real actor)')
