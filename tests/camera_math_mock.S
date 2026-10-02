.syntax unified
.arm
.text
.global tanf,atanf
.type tanf,%function
tanf: bx lr
.type atanf,%function
atanf: bx lr
.section .note.GNU-stack,"",%progbits
