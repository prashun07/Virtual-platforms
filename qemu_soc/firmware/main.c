/*
 * Bare-metal firmware for systemc-soc.
 * Programs the USER SystemC model through the QEMU remote-mmio window at
 * 0x40000000 (forwarded 1:1 to the SystemC peripheral offsets).
 */

#include <stdint.h>
#include "timer_regs.h"
#include "semihost.h"

static int wait_bit(volatile uint32_t *reg, uint32_t mask, uint32_t spins)
{
    while (spins--) {
        if ((*reg & mask) != 0) {
            return 0;
        }
    }
    return -1;
}

int main(void)
{
    sh_puts("cosim: baremetal <-> QEMU <-> SystemC Timer\n");

    TIMER_REG_CTRL = 0;
    TIMER_REG_VALUE = 0;
    TIMER_REG_CMP = 0;
    TIMER_REG_INTR = 0;

    /* Enable + compare + overflow; compare match at count 20 */
    TIMER_REG_CMP = 20;
    TIMER_REG_CTRL = TIMER_CTRL_ENABLE | TIMER_CTRL_CMP_EN | TIMER_CTRL_OV_EN;

    if (wait_bit(&TIMER_REG_INTR, TIMER_INTR_CMP, 1000000) != 0) {
        sh_puts("FAIL: compare status timeout\n");
        sh_exit(1);
    }
    sh_puts("PASS: compare status set\n");
    TIMER_REG_INTR = TIMER_REG_INTR & ~TIMER_INTR_CMP;

    TIMER_REG_VALUE = 250;
    if (wait_bit(&TIMER_REG_INTR, TIMER_INTR_OV, 1000000) != 0) {
        sh_puts("FAIL: overflow status timeout\n");
        sh_exit(1);
    }
    sh_puts("PASS: overflow status set\n");

    TIMER_REG_CTRL = 0;
    uint32_t before = TIMER_REG_VALUE;
    for (volatile int i = 0; i < 1000; i++) {
    }
    uint32_t after = TIMER_REG_VALUE;
    if (before != after) {
        sh_puts("FAIL: timer still running\n");
        sh_exit(1);
    }
    sh_puts("PASS: timer stopped when disabled\n");

    sh_puts("ALL TESTS COMPLETED\n");
    sh_exit(0);
    return 0;
}
