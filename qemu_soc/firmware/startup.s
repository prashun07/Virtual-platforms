/* Cortex-M3 startup for systemc-soc (QEMU). */

  .syntax unified
  .cpu cortex-m3
  .thumb

  .global g_pfnVectors
  .global Default_Handler
  .global Reset_Handler

  .section .isr_vector, "a", %progbits
  .type g_pfnVectors, %object
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word Default_Handler          /* NMI */
  .word Default_Handler          /* HardFault */
  .word Default_Handler          /* MemManage */
  .word Default_Handler          /* BusFault */
  .word Default_Handler          /* UsageFault */
  .word 0
  .word 0
  .word 0
  .word 0
  .word Default_Handler          /* SVCall */
  .word Default_Handler          /* DebugMon */
  .word 0
  .word Default_Handler          /* PendSV */
  .word Default_Handler          /* SysTick */
  .word TimerCmp_Handler         /* IRQ0: compare */
  .word TimerOv_Handler          /* IRQ1: overflow */
  .size g_pfnVectors, .-g_pfnVectors

  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
1:
  cmp r0, r1
  bcc 2f
  b 3f
2:
  ldr r3, [r2], #4
  str r3, [r0], #4
  b 1b
3:
  ldr r0, =_sbss
  ldr r1, =_ebss
  movs r2, #0
4:
  cmp r0, r1
  bcc 5f
  b 6f
5:
  str r2, [r0], #4
  b 4b
6:
  bl main
  b Default_Handler

  .weak Default_Handler
  .type Default_Handler, %function
Default_Handler:
  b Default_Handler

  .weak TimerCmp_Handler
  .thumb_set TimerCmp_Handler, Default_Handler

  .weak TimerOv_Handler
  .thumb_set TimerOv_Handler, Default_Handler
