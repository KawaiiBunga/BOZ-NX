.syntax unified
.arm
.text
.global native_dispatch
.type native_dispatch,%function
native_dispatch:
  bx lr
.section .note.GNU-stack,"",%progbits
