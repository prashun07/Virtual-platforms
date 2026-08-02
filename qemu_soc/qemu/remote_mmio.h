#ifndef HW_MISC_REMOTE_MMIO_H
#define HW_MISC_REMOTE_MMIO_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_REMOTE_MMIO "remote-mmio"
OBJECT_DECLARE_SIMPLE_TYPE(RemoteMmioState, REMOTE_MMIO)

#define REMOTE_MMIO_MAX_IRQ 8

struct RemoteMmioState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irqs[REMOTE_MMIO_MAX_IRQ];

    char *socket_path;
    uint64_t size;
    int fd;
};

#endif /* HW_MISC_REMOTE_MMIO_H */
