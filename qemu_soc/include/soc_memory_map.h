#ifndef SOC_MEMORY_MAP_H
#define SOC_MEMORY_MAP_H

/*
 * Shared memory map constants for QEMU machines and SystemC PL registration.
 *
 * systemc-soc  — Cortex-M3 microcontroller class (remote-mmio @ PL base)
 * systemc-ps   — Cortex-A9 application processor class (+ UART, GIC, DDR)
 */

/* Programmable Logic (SystemC peripherals via remote-mmio) */
#define SYSTEMC_PL_M_PROFILE_BASE  0x40000000ull
#define SYSTEMC_PL_A_PROFILE_BASE  0xF0000000ull
#define SYSTEMC_PL_WINDOW_SIZE     0x00100000ull  /* 1 MiB decode window */

/* systemc-ps (Cortex-A9) — QEMU-side map */
#define PS_DDR_BASE                0x60000000ull
#define PS_GIC_DIST_BASE           0x1e001000ull  /* inside A9MPCore private region */
#define PS_UART0_BASE              0x10009000ull  /* PL011, vexpress-compatible */
#define PS_UART0_IRQ               5

/* systemc-soc (Cortex-M3) */
#define M3_FLASH_BASE              0x00000000ull
#define M3_SRAM_BASE               0x20000000ull

#endif /* SOC_MEMORY_MAP_H */
