.syntax unified
.thumb
.text
.global render_tail
.thumb_func
render_tail:
  ldr r12,=render_args
  stmia r12,{r0-r3}
  str r4,[r12,#16]
  eor r0,r0,r1
  eor r0,r0,r2
  eor r0,r0,r3
  add sp,sp,#24
  pop.w {r4-r10,pc}
.section .note.GNU-stack,"",%progbits
