/*
 * remote_mmio.h
 *
 * Copyright (c) 2026 Prashun Jha. All rights reserved.
 *
 * @author Prashun Jha
 *
 * QEMU SysBus device that proxies guest MMIO to the SystemC cosim server.
 *
 * When firmware performs a load or store to the peripheral window (e.g.
 * 0x40000000 on systemc-soc), QEMU invokes remote_mmio_read/write() in
 * remote_mmio.c. Those callbacks forward the access over a Unix domain socket
 * using the SCM1 protocol (see cosim_protocol.h). No Timer or other IP logic
 * lives in this device — only transport between guest memory space and SystemC.
 *
 * Instantiated by systemc_soc.c and systemc_ps.c; socket path comes from the
 * "socket" device property (typically SYSTEMC_COSIM_SOCKET).
 */

#ifndef HW_MISC_REMOTE_MMIO_H
#define HW_MISC_REMOTE_MMIO_H

#include "hw/sysbus.h"
#include "qom/object.h"

/* QOM type name; used with qdev_new(TYPE_REMOTE_MMIO) in machine init code. */
#define TYPE_REMOTE_MMIO "remote-mmio"
OBJECT_DECLARE_SIMPLE_TYPE(RemoteMmioState, REMOTE_MMIO)

/* IRQ outputs reserved for future SystemC -> QEMU interrupt notification. */
#define REMOTE_MMIO_MAX_IRQ 8

/*
 * RemoteMmioState — QEMU SysBus device (the MMIO bridge in cosim).
 *
 * Guest CPU accesses hit iomem -> remote_mmio_ops -> socket RPC -> CosimServer
 * on the SystemC side. Inherits SysBusDevice for memory mapping and IRQ lines.
 */
struct RemoteMmioState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;                 /* MMIO region visible to the guest CPU */
    qemu_irq irqs[REMOTE_MMIO_MAX_IRQ]; /* outbound IRQ lines (async path TBD) */

    char *socket_path; /* Unix socket path, e.g. /tmp/systemc_cosim.sock */
    uint64_t size;     /* MMIO window size (device property, default 4 KiB) */
    int fd;            /* connected socket fd; -1 when disconnected */
};

#endif /* HW_MISC_REMOTE_MMIO_H */
