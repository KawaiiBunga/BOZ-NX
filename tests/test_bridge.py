"""Execute the production ARM bridge under Unicorn, including Thumb returns.
Unicorn is used only by this PC test; it is never part of the Switch build.
"""
from pathlib import Path
import struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import *

root=Path(__file__).resolve().parents[1]
symbols={}
for line in (root/'test-artifacts/bridge.nm').read_text().splitlines():
    fields=line.split()
    if len(fields)==3: symbols[fields[2]]=int(fields[0],16)
code=(root/'test-artifacts/bridge.bin').read_bytes()
regs=[UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R4,
      UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,
      UC_ARM_REG_R10,UC_ARM_REG_R11,UC_ARM_REG_R12]

def exercise(slot, alignment, thumb_return, hook):
    u=Uc(UC_ARCH_ARM,UC_MODE_ARM)
    u.mem_map(0x100000,0x40000);u.mem_write(0x100000,code)
    u.mem_map(0x300000,0x10000)
    stop=0x130000
    sp=0x30ff00-alignment
    values=[0x12340000+i for i in range(13)]
    for r,v in zip(regs,values):u.reg_write(r,v)
    flags=0xa8050010
    u.reg_write(UC_ARM_REG_CPSR,flags)
    u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
    u.reg_write(UC_ARM_REG_FPSCR,0)
    ds=[0x3ff0000000000000+i for i in range(32)]
    for i,v in enumerate(ds):u.reg_write(UC_ARM_REG_D0+i,v)
    u.reg_write(UC_ARM_REG_SP,sp)
    u.reg_write(UC_ARM_REG_LR,stop|int(thumb_return))
    u.mem_write(sp,struct.pack('<4I',5,6,7,8))
    seen=[]
    def intercept(uc,addr,size,unused):
        if addr!=symbols['native_dispatch']:return
        assert uc.reg_read(UC_ARM_REG_SP)%8==0,'C ABI stack alignment'
        frame=uc.reg_read(UC_ARM_REG_R0)
        assert uc.reg_read(UC_ARM_REG_R1)==slot
        state=list(struct.unpack('<16I',uc.mem_read(frame,64)))
        assert state[:13]==values and state[13]==sp and state[14]==stop|int(thumb_return)
        assert struct.unpack('<4I',uc.mem_read(state[13],16))==(5,6,7,8),'stack arguments'
        assert struct.unpack('<I',uc.mem_read(frame+64,4))[0]&0xf80f0000==flags&0xf80f0000
        if hook:
            assert list(struct.unpack('<32Q',uc.mem_read(frame+68,256)))==ds
            # The C hook edits core/VFP/flags and selects a continuation.
            uc.mem_write(frame+68+31*8,struct.pack('<Q',0x1111222233334444))
            uc.mem_write(frame+64,struct.pack('<I',0x600a0010))
        # Model an arbitrary ABI-conforming C handler clobbering caller-save VFP.
        for i in list(range(8))+list(range(16,32)):uc.reg_write(UC_ARM_REG_D0+i,0)
        state[0]=0xdeadbeef;state[1]=0xaabbccdd;state[15]=stop|int(thumb_return)
        uc.mem_write(frame,struct.pack('<16I',*state));seen.append(slot)
    u.hook_add(UC_HOOK_CODE,intercept)
    base=symbols['native_hook_veneers'] if hook else symbols['native_veneers']
    index=slot-512 if hook else slot
    u.emu_start(base+index*12,stop,count=500)
    assert seen==[slot]
    assert u.reg_read(UC_ARM_REG_PC)==stop
    assert bool(u.reg_read(UC_ARM_REG_CPSR)&32)==thumb_return
    assert u.reg_read(UC_ARM_REG_SP)==sp and u.reg_read(UC_ARM_REG_LR)==stop|int(thumb_return)
    assert u.reg_read(UC_ARM_REG_R0)==0xdeadbeef and u.reg_read(UC_ARM_REG_R1)==0xaabbccdd
    assert [u.reg_read(r) for r in regs[2:]]==values[2:]
    for i in range(8,16):assert u.reg_read(UC_ARM_REG_D0+i)==ds[i]
    if hook:
        for i in range(31):assert u.reg_read(UC_ARM_REG_D0+i)==ds[i]
        assert u.reg_read(UC_ARM_REG_D31)==0x1111222233334444
        assert u.reg_read(UC_ARM_REG_CPSR)&0xf80f0000==0x600a0010&0xf80f0000

count=0
for hook in [False,True]:
    for slot in ([0,1,255,511] if not hook else [512,515,516,519,520,521]):
        for alignment in [0,4]:
            for thumb in [False,True]:
                exercise(slot,alignment,thumb,hook);count+=1
def exercise_entry(early_exit):
    u=Uc(UC_ARCH_ARM,UC_MODE_ARM)
    u.mem_map(0x100000,0x40000);u.mem_write(0x100000,code)
    u.mem_map(0x300000,0x20000)
    sp=0x30ff00;game_sp=0x31ff00;stop=0x130000;game_pc=0x132000
    saved=[0x88770000+i for i in range(8)]
    for r,v in zip(regs[4:12],saved):u.reg_write(r,v)
    ds=[0x1122334400000000+i for i in range(8)]
    u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
    for i,v in enumerate(ds):u.reg_write(UC_ARM_REG_D8+i,v)
    u.reg_write(UC_ARM_REG_SP,sp);u.reg_write(UC_ARM_REG_LR,stop)
    u.reg_write(UC_ARM_REG_R0,game_pc);u.reg_write(UC_ARM_REG_R1,game_sp)
    u.mem_write(game_pc,struct.pack('<I',0xe12fff1e)) # bx lr
    def visit(uc,addr,size,unused):
        if addr!=game_pc:return
        assert uc.reg_read(UC_ARM_REG_SP)==game_sp
        for i in range(8):uc.reg_write(UC_ARM_REG_D8+i,0)
        if early_exit:
            for r in regs[4:12]:uc.reg_write(r,0)
            uc.reg_write(UC_ARM_REG_PC,symbols['native_leave'])
    u.hook_add(UC_HOOK_CODE,visit)
    u.emu_start(symbols['native_call_entry'],stop,count=500)
    assert u.reg_read(UC_ARM_REG_SP)==sp
    assert [u.reg_read(r) for r in regs[4:12]]==saved
    assert [u.reg_read(UC_ARM_REG_D8+i) for i in range(8)]==ds
exercise_entry(False);exercise_entry(True)
print(f'PASS: {count} production bridge cases plus entry/early exit: ABI, stack args, ARM/Thumb, CPSR, VFP, 4/8-byte stacks')
