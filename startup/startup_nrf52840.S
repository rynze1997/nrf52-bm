.syntax unified
.cpu cortex-m4
.thumb

.section .isr_vector, "a", %progbits
.word _estack
.word Reset_Handler
.rept 14
.word Halt
.endr

.text
.thumb_func
.global Reset_Handler
Reset_Handler:
    bl main
    b Halt

.thumb_func
.global Halt
Halt:
    b Halt
