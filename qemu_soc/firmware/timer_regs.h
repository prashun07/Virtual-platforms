#ifndef TIMER_REGS_H
#define TIMER_REGS_H

#include <stdint.h>

/* Same MMIO layout as systemc_model/Timer (and QEMU systemc-timer device). */
#define TIMER_BASE        0x40000000u

#define TIMER_REG_CTRL    (*(volatile uint32_t *)(TIMER_BASE + 0x00))
#define TIMER_REG_VALUE   (*(volatile uint32_t *)(TIMER_BASE + 0x04))
#define TIMER_REG_CMP     (*(volatile uint32_t *)(TIMER_BASE + 0x08))
#define TIMER_REG_INTR    (*(volatile uint32_t *)(TIMER_BASE + 0x0C))

#define TIMER_CTRL_ENABLE (1u << 0)
#define TIMER_CTRL_CMP_EN (1u << 1)
#define TIMER_CTRL_OV_EN  (1u << 2)

#define TIMER_INTR_CMP    (1u << 1)
#define TIMER_INTR_OV     (1u << 2)

#endif /* TIMER_REGS_H */
