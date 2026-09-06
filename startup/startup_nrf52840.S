.syntax unified
.cpu cortex-m4
.thumb

.section .isr_vector, "a", %progbits
.word _estack
.word Reset_Handler
.rept 14                 /* NMI .. SysTick */
.word Halt
.endr
.rept 2                  /* IRQ 0 .. 1 */
.word Halt
.endr
.word UART0_UARTE0_IRQHandler  /* IRQ 2 */
.rept 5                  /* IRQ 3 .. 7 */
.word Halt
.endr
.word TIMER0_IRQHandler  /* IRQ 8 */

.text
.thumb_func
.global Reset_Handler
Reset_Handler:
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
.Lclear_bss:
    cmp r0, r1
    beq .Lrun
    strb r2, [r0]
    adds r0, #1
    b .Lclear_bss
.Lrun:
    bl main
    b Halt

.thumb_func
.global Halt
Halt:
    b Halt
