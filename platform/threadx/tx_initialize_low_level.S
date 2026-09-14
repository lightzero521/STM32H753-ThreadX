#define SYSTEM_CLOCK_HZ 480000000
#define SYSTICK_HZ      1000
#define SYSTICK_RELOAD  ((SYSTEM_CLOCK_HZ / SYSTICK_HZ) - 1)

    .syntax unified
    .thumb
    .section .text._tx_initialize_low_level,"ax",%progbits
    .balign 4
    .global _tx_initialize_low_level
    .type _tx_initialize_low_level, %function
    .thumb_func
_tx_initialize_low_level:
    cpsid   i

    ldr     r0, =_tx_initialize_unused_memory
    ldr     r1, =_ebss
    adds    r1, r1, #7
    bic     r1, r1, #7
    str     r1, [r0]

    ldr     r0, =0xE000ED08
    ldr     r1, =g_pfnVectors
    str     r1, [r0]
    ldr     r0, =_tx_thread_system_stack_ptr
    ldr     r1, [r1]
    str     r1, [r0]

    ldr     r0, =0xE000E014
    ldr     r1, =SYSTICK_RELOAD
    str     r1, [r0]
    ldr     r0, =0xE000E010
    movs    r1, #7
    str     r1, [r0]

    ldr     r0, =0xE000ED20
    ldr     r1, =0x40FF0000
    str     r1, [r0]
    bx      lr
.size _tx_initialize_low_level, .-_tx_initialize_low_level

    .section .text.SysTick_Handler,"ax",%progbits
    .balign 4
    .global SysTick_Handler
    .type SysTick_Handler, %function
    .thumb_func
SysTick_Handler:
    push    {r0, lr}
    bl      _tx_timer_interrupt
    pop     {r0, lr}
    bx      lr
.size SysTick_Handler, .-SysTick_Handler

    .section .note.GNU-stack,"",%progbits
