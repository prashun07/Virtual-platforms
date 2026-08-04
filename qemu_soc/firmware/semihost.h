#ifndef SEMIHOST_H
#define SEMIHOST_H

#include <stdint.h>

/*
 * ARM T32 semihosting (bkpt 0xAB) for QEMU system-mode cosim.
 *
 * QEMU returns 0xdeadbeef in r0 after SYS_WRITE0. Mark r0/r1 clobbered so
 * the compiler reloads them before every call.
 */

#define ADP_STOPPED_APPLICATION_EXIT 0x20026u

static inline void semihost_call(uint32_t op, uint32_t arg)
{
    asm volatile (
        "mov r0, %[op]\n"
        "mov r1, %[arg]\n"
        "bkpt #0xAB"
        :
        : [op] "r" (op), [arg] "r" (arg)
        : "r0", "r1", "memory");
}

static inline void sh_puts(const char *s)
{
    semihost_call(0x04u, (uint32_t)(uintptr_t)s); /* SYS_WRITE0 */
}

static inline void sh_exit(int code)
{
    if (code == 0) {
        /* T32 SYS_EXIT: r1 = reason code (not a pointer). */
        semihost_call(0x18u, ADP_STOPPED_APPLICATION_EXIT);
    } else {
        uint32_t args[2] = { ADP_STOPPED_APPLICATION_EXIT, (uint32_t)code };
        semihost_call(0x20u, (uint32_t)(uintptr_t)args); /* SYS_EXIT_EXTENDED */
    }
}

#endif /* SEMIHOST_H */
