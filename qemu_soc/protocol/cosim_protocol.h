/*
 * Shared QEMU <-> SystemC cosimulation protocol (Unix socket, little-endian).
 *
 * This is an interface only — no peripheral behavior lives here.
 */

#ifndef QEMU_COSIM_PROTOCOL_H
#define QEMU_COSIM_PROTOCOL_H

#include <stdint.h>

#define COSIM_MAGIC   0x53434D31u  /* 'SCM1' */
#define COSIM_VERSION 1u

enum CosimOp {
    COSIM_OP_READ  = 1,
    COSIM_OP_WRITE = 2,
    COSIM_OP_TICK  = 3,  /* advance SystemC clocks; count in req.data */
    COSIM_OP_QUIT  = 4,
};

enum CosimStatus {
    COSIM_OK    = 0,
    COSIM_ERROR = 1,
};

#pragma pack(push, 1)
struct CosimRequest {
    uint32_t magic;
    uint32_t version;
    uint32_t op;
    uint32_t addr;   /* byte offset within peripheral window */
    uint32_t data;   /* write data, or tick count */
};

struct CosimResponse {
    uint32_t magic;
    uint32_t version;
    uint32_t status;
    uint32_t data;   /* read data */
};

/* SystemC -> QEMU asynchronous IRQ notification (optional second channel). */
struct CosimIrqEvent {
    uint32_t magic;
    uint32_t version;
    uint32_t irq;    /* irq index */
    uint32_t level;  /* 0 or 1 */
};
#pragma pack(pop)

#endif /* QEMU_COSIM_PROTOCOL_H */
