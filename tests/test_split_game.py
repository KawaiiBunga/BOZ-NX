"""Validate split-probe anchors against the owner's supported game image.
Execute the actual renderer entry and relocated entry through the production
bridge/helper with a synthetic return body. This tests the hook boundary, not
the original rendering body, a second actor, GPU drawing or real networking.
"""
from pathlib import Path
import struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import *

BASE, ENTRY, POOL = 0x800000, 0x193060, 0x193270
game=Path('test-artifacts/game-image.bin').read_bytes()
code=Path('test-artifacts/render.bin').read_bytes()
symbols={}
for line in Path('test-artifacts/render.nm').read_text().splitlines():
    p=line.split()
    if len(p)==3: symbols[p[2]]=int(p[0],16)
def word(data, address): return struct.unpack_from('<I',data,address)[0]
anchors={
    0x400798:'17CPlayerController', 0x401968:'13CRemotePlayer',
    0x4069d8:'14CWeaponManager', 0x406b10:'6CWorld',
    0x3fa678:'16CGameStateIngame',0x3fd1c8:'13CInputManager',
    0x3fa418:'12CGameNetwork',0x3e6290:'9CIsCamera',
}
for vtable,name in anchors.items():
    assert word(game,vtable-8)==0
    typeinfo=word(game,vtable-4)-BASE
    nameptr=word(game,typeinfo+4)-BASE
    assert game[nameptr:nameptr+len(name)+1]==name.encode()+b'\0',name
assert word(game,0x3fa678+0x18)==BASE+ENTRY+1
assert game[ENTRY:ENTRY+8]==bytes.fromhex('2de9f04786b0824c')
assert word(game,POOL)==0x2d47a4

def run(patched,alignment):
    u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.ctl_set_cpu_model(UC_CPU_ARM_MAX)
    u.mem_map(BASE,0x600000);u.mem_write(BASE,game)
    u.mem_map(0x100000,0x60000);u.mem_write(0x100000,code)
    u.mem_map(0x300000,0x20000)
    # Synthetic body records original arguments/literal and unwinds the game's
    # verified prologue. Both plain and wrapped paths execute that same body.
    u.mem_write(BASE+ENTRY+8,struct.pack('<II',0xf000f8df,symbols['render_tail']|1))
    trampoline=game[ENTRY:ENTRY+6]+struct.pack('<H',0x4c02)
    trampoline+=struct.pack('<III',0xf000f8df,BASE+ENTRY+9,word(game,POOL))
    u.mem_write(0x130100,trampoline)
    if patched:
        u.mem_write(BASE+ENTRY,struct.pack('<II',0xf000f8df,symbols['native_hook_veneers']+9*12))
    sp,stop=0x31f000-alignment,0x13ff00
    u.reg_write(UC_ARM_REG_SP,sp);u.reg_write(UC_ARM_REG_LR,stop|1)
    u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
    args=[0x302000,0x22222222,0x33333333,0x44444444]
    for r,v in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3],args):u.reg_write(r,v)
    saved={r:0xaaaa0000+r for r in [UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,
                                 UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11]}
    for r,v in saved.items():u.reg_write(r,v)
    u.emu_start(BASE+ENTRY+1,stop,count=50000)
    assert u.reg_read(UC_ARM_REG_PC)==stop
    assert u.reg_read(UC_ARM_REG_SP)==sp
    for r,v in saved.items():assert u.reg_read(r)==v
    observed=struct.unpack('<5I',u.mem_read(symbols['render_args'],20))
    assert list(observed[:4])==args and observed[4]==word(game,POOL)
    assert u.reg_read(UC_ARM_REG_R0)==args[0]^args[1]^args[2]^args[3]
    return observed
for alignment in (0,4):assert run(False,alignment)==run(True,alignment)
print('PASS: split-probe RTTI/vtable anchors, renderer signature/literal, production hook ABI and relocated entry; rendering body not exercised')

def manager_links(camera_present):
    """Execute the real init's reference-resolution section, stopping before
    subsequent weapon/event setup. The component registry, reference observers
    and event subscriptions use fixtures; this does not construct a real actor.
    The lookup wrappers and reference assignment execute their original code.
    """
    u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.ctl_set_cpu_model(UC_CPU_ARM_MAX)
    u.mem_map(BASE,0x600000);u.mem_write(BASE,game)
    u.mem_map(0x300000,0x20000)
    manager,entity,hierarchy,parent,parent_hierarchy=0x302000,0x303000,0x304000,0x305000,0x306000
    transform,controller,camera_entity,camera,camera_transform=0x307000,0x308000,0x309000,0x30a000,0x30b000
    def store(address,value):u.mem_write(address,struct.pack('<I',value))
    def load(address):return struct.unpack('<I',u.mem_read(address,4))[0]
    store(manager+0xc,entity);store(entity+4,101);store(parent+4,102);store(camera_entity+4,103)
    store(hierarchy+0x20,parent_hierarchy);store(parent_hierarchy+0xc,parent)
    # Each registry is the literal-relative address used by its real wrapper.
    def registry(add_pc,literal,offset):return (BASE+add_pc+4+word(game,literal)+offset)&0xffffffff
    lookups={
        (registry(0xbc6b4,0xbc6c0,4),101):hierarchy,
        (registry(0xc23ec,0xc23f8,8),102):transform,
        (registry(0x1d38d4,0x1d38e0,0x10),102):controller,
        (registry(0xb88c4,0xb88d0,4),102):0,
        (registry(0x1cbc34,0x1cbc40,4),102):0,
        (registry(0xb941c,0xb9428,4),103):camera,
        (registry(0xc23ec,0xc23f8,8),103):camera_transform,
    }
    seen=[]
    def intercept(uc,address,size,user):
        rva=address-BASE
        if rva==0x4eeb0:
            key=(uc.reg_read(UC_ARM_REG_R0),uc.reg_read(UC_ARM_REG_R1))
            assert key in lookups,tuple(hex(v) for v in key)
            seen.append(key);uc.reg_write(UC_ARM_REG_R0,lookups[key])
        elif rva==0xbd538:
            text=bytes(uc.mem_read(uc.reg_read(UC_ARM_REG_R0),11))
            assert text==b'MainCamera\0'
            uc.reg_write(UC_ARM_REG_R0,camera_entity if camera_present else 0)
        elif rva in (0xb9ec0,0xc1aca):
            pass # reference observer registration and event subscription fixtures
        else:return
        uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
    u.hook_add(UC_HOOK_CODE,intercept)
    u.reg_write(UC_ARM_REG_SP,0x31f000);u.reg_write(UC_ARM_REG_R0,manager)
    u.emu_start(BASE+0x2225c5,BASE+0x222662 if camera_present else BASE+0x22266e,count=50000)
    assert load(manager+0x48)==parent
    assert load(manager+0x50)==transform and load(manager+0x60)==controller
    assert load(manager+0x78)==(camera if camera_present else 0)
    assert load(manager+0x80)==(camera_transform if camera_present else 0)
    assert len(seen)==(7 if camera_present else 5)

manager_links(False);manager_links(True)
print('PASS: actual weapon-manager init reference section distinguishes player entity, transform, controller and camera links; registries/events are fixtures')
