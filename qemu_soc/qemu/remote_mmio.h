/*
 * remote_mmio.h — QEMU device that proxies guest MMIO to SystemC.
 *
 * When firmware does a load/store to the peripheral window (e.g. 0x40000000),
 * QEMU calls remote_mmio_read/write(), which forwards the access over a Unix
 * socket to CosimServer on the SystemC side. No Timer (or other IP) logic lives
 * here — only transport.
 */

#ifndef HW_MISC_REMOTE_MMIO_H
#define HW_MISC_REMOTE_MMIO_H

#include "hw/sysbus.h"
#include "qom/object.h"

/* QOM type name; used with qdev_new(TYPE_REMOTE_MMIO) in systemc_soc.c */
#define TYPE_REMOTE_MMIO "remote-mmio"
OBJECT_DECLARE_SIMPLE_TYPE(RemoteMmioState, REMOTE_MMIO)

/* IRQ outputs reserved for future SystemC -> QEMU interrupt notification */
#define REMOTE_MMIO_MAX_IRQ 8

/*
 * RemoteMmioState — QEMU SysBus device (the "bridge" in the cosim architecture).
 *
 * Inherits SysBusDevice so it can be memory-mapped and expose IRQ lines.
 * Guest CPU MMIO hits iomem -> remote_mmio_ops -> socket -> SystemC.
 */
struct RemoteMmioState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;              /* MMIO region visible to the guest CPU */
    qemu_irq irqs[REMOTE_MMIO_MAX_IRQ]; /* outbound IRQ lines (not used yet) */

    char *socket_path;  /* Unix socket path, e.g. /tmp/systemc_cosim.sock */
    uint64_t size;      /* size of the MMIO window (set via device property) */
    int fd;             /* connected socket fd; -1 when disconnected */
};

#endif /* HW_MISC_REMOTE_MMIO_H */
