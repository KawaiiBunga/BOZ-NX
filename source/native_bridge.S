/* Register frame must match native_exec.c. Imports use softfp AAPCS;
 * mid-function hooks additionally preserve all VFP state and condition flags. */
.syntax unified
.arm
.fpu neon-fp-armv8
.text
.balign 4
.global native_veneers
.type native_veneers,%function
native_veneers:
.set slot,0
.rept 512
  push {r12,lr}
  movw r12,#slot
  b native_import_gate
  .set slot,slot+1
.endr
.size native_veneers,.-native_veneers
.global native_hook_veneers
.type native_hook_veneers,%function
native_hook_veneers:
.set slot,512
.rept 10
  push {r12,lr}
  movw r12,#slot
  b native_hook_gate
  .set slot,slot+1
.endr
.size native_hook_veneers,.-native_hook_veneers

.macro capture
  sub sp,sp,#336
  stmia sp,{r0-r11}
  mrs r0,cpsr
  str r0,[sp,#64]
  ldr r0,[sp,#336]
  str r0,[sp,#48]
  add r0,sp,#344
  str r0,[sp,#52]
  ldr r0,[sp,#340]
  str r0,[sp,#56]
  mov r1,r12
  mov r4,sp
  bic sp,sp,#7
  mov r0,r4
.endm
.macro restore
  mov sp,r4
  ldr r0,[sp,#64]
  msr APSR_nzcvqg,r0
  ldr r0,[sp,#60]
  str r0,[sp,#340]
  ldmia sp,{r0-r12}
  ldr lr,[sp,#56]
  add sp,sp,#340
  ldr pc,[sp],#4
.endm
.type native_import_gate,%function
native_import_gate:
  capture
  bl native_dispatch
  restore
.size native_import_gate,.-native_import_gate
.type native_hook_gate,%function
native_hook_gate:
  capture
  add r2,r4,#68
  vstmia r2!,{d0-d15}
  vstmia r2!,{d16-d31}
  vmrs r3,fpscr
  str r3,[r4,#324]
  bl native_dispatch
  ldr r3,[r4,#324]
  vmsr fpscr,r3
  add r2,r4,#68
  vldmia r2!,{d0-d15}
  vldmia r2!,{d16-d31}
  restore
.size native_hook_gate,.-native_hook_gate

.global native_call_entry
.type native_call_entry,%function
native_call_entry:
  push {r4-r11,lr}
  sub sp,sp,#4
  vpush {d8-d15}
  mov r4,sp
  ldr r2,=native_host_stack
  str sp,[r2]
  mov r12,r0
  bic sp,r1,#7
  mov r0,#0
  mov r1,#0
  mov r2,#0
  mov r3,#0
  blx r12
  mov sp,r4
  vpop {d8-d15}
  add sp,sp,#4
  pop {r4-r11,pc}
.size native_call_entry,.-native_call_entry
.global native_leave
.type native_leave,%function
native_leave:
  ldr r0,=native_host_stack
  ldr sp,[r0]
  vpop {d8-d15}
  add sp,sp,#4
  pop {r4-r11,pc}
.size native_leave,.-native_leave
.bss
.balign 4
native_host_stack: .space 4
.section .note.GNU-stack,"",%progbits
