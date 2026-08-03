/*
 * remote_mmio.c — implementation of the QEMU remote-MMIO bridge.
 *
 * Data path (write example):
 *   Guest STR to 0x40000000
 *     -> remote_mmio_write()
 *     -> remote_mmio_transact(COSIM_OP_WRITE)
 *     -> send CosimRequest on Unix socket
 *     -> CosimServer (SystemC) -> MmioAdapter -> Timer::bus_thread()
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/misc/remote_mmio.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "hw/misc/cosim_protocol.h"

/*
 * remote_mmio_connect — open a stream connection to the SystemC cosim server.
 *
 * Called lazily on the first MMIO access. The SystemC process must already be
 * listening on socket_path (CosimServer::start_listening).
 * Returns 0 on success, -1 on failure.
 */
static int remote_mmio_connect(RemoteMmioState *s)
{
    struct sockaddr_un addr;
    int fd;

    if (s->fd >= 0) {
        return 0; /* already connected */
    }
    if (!s->socket_path || !s->socket_path[0]) {
        error_report("remote-mmio: socket path not set");
        return -1;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        error_report("remote-mmio: socket(): %s", strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strlen(s->socket_path) >= sizeof(addr.sun_path)) {
        error_report("remote-mmio: socket path too long");
        close(fd);
        return -1;
    }
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", s->socket_path);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        error_report("remote-mmio: connect(%s): %s",
                     s->socket_path, strerror(errno));
        close(fd);
        return -1;
    }

    s->fd = fd;
    return 0;
}

/*
 * remote_mmio_transact — send one cosim request and wait for the response.
 *
 * This is the core RPC between QEMU and SystemC:
 *   op   = COSIM_OP_READ | WRITE | TICK | QUIT
 *   addr = byte offset within the peripheral window (0x00, 0x04, …)
 *   wdata = value for writes (ignored on reads)
 *   rdata = optional out-pointer; filled with read data on COSIM_OP_READ
 *
 * Blocks until CosimServer replies with CosimResponse.
 */
static int remote_mmio_transact(RemoteMmioState *s, uint32_t op,
                                uint32_t addr, uint32_t wdata,
                                uint32_t *rdata)
{
    struct CosimRequest req;
    struct CosimResponse resp;
    size_t got;
    uint8_t *p;
    ssize_t n;

    if (remote_mmio_connect(s) < 0) {
        return -1;
    }

    memset(&req, 0, sizeof(req));
    req.magic = COSIM_MAGIC;
    req.version = COSIM_VERSION;
    req.op = op;
    req.addr = addr;
    req.data = wdata;

    /* Send full request packet (may require multiple send() calls) */
    p = (uint8_t *)&req;
    got = 0;
    while (got < sizeof(req)) {
        n = send(s->fd, p + got, sizeof(req) - got, 0);
        if (n <= 0) {
            error_report("remote-mmio: send failed");
            close(s->fd);
            s->fd = -1;
            return -1;
        }
        got += (size_t)n;
    }

    /* Block until SystemC returns CosimResponse */
    p = (uint8_t *)&resp;
    got = 0;
    while (got < sizeof(resp)) {
        n = recv(s->fd, p + got, sizeof(resp) - got, 0);
        if (n <= 0) {
            error_report("remote-mmio: recv failed");
            close(s->fd);
            s->fd = -1;
            return -1;
        }
        got += (size_t)n;
    }

    if (resp.magic != COSIM_MAGIC || resp.version != COSIM_VERSION ||
        resp.status != COSIM_OK) {
        error_report("remote-mmio: bad response");
        return -1;
    }

    if (rdata) {
        *rdata = resp.data;
    }
    return 0;
}

/*
 * remote_mmio_read — QEMU MemoryRegion read callback.
 *
 * Invoked when guest firmware executes LDR from the bridge window.
 * offset is the register offset (not the absolute CPU address).
 * On the SystemC side this becomes MmioAdapter::bus_read(offset).
 */
static uint64_t remote_mmio_read(void *opaque, hwaddr offset, unsigned size)
{
    RemoteMmioState *s = opaque;
    uint32_t data = 0;

    /* Timer model uses 32-bit aligned word accesses only */
    if (size != 4 || (offset & 3)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "remote-mmio: unsupported read @0x%" HWADDR_PRIx
                      " size=%u\n", offset, size);
        return 0;
    }

    if (remote_mmio_transact(s, COSIM_OP_READ, (uint32_t)offset, 0, &data) < 0) {
        return 0;
    }
    return data;
}

/*
 * remote_mmio_write — QEMU MemoryRegion write callback.
 *
 * Invoked when guest firmware executes STR to the bridge window.
 * On the SystemC side this becomes MmioAdapter::bus_write(offset, value),
 * which toggles write_en/address/data_in and wakes Timer::bus_thread().
 */
static void remote_mmio_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    RemoteMmioState *s = opaque;

    if (size != 4 || (offset & 3)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "remote-mmio: unsupported write @0x%" HWADDR_PRIx
                      " size=%u\n", offset, size);
        return;
    }

    remote_mmio_transact(s, COSIM_OP_WRITE, (uint32_t)offset,
                         (uint32_t)value, NULL);
}

/* Hooks registered with QEMU's memory subsystem for the iomem region */
static const MemoryRegionOps remote_mmio_ops = {
    .read = remote_mmio_read,
    .write = remote_mmio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

/*
 * remote_mmio_instance_init — QOM instance constructor.
 *
 * Runs when the device object is created. Initializes socket fd and
 * registers IRQ output lines with the SysBus layer.
 */
static void remote_mmio_instance_init(Object *obj)
{
    RemoteMmioState *s = REMOTE_MMIO(obj);
    int i;

    s->fd = -1;
    for (i = 0; i < REMOTE_MMIO_MAX_IRQ; i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irqs[i]);
    }
}

/*
 * remote_mmio_realize — called when the device is fully configured and started.
 *
 * Creates the MMIO MemoryRegion and exposes it on the SysBus so the machine
 * can map it at 0x40000000 via sysbus_mmio_map().
 */
static void remote_mmio_realize(DeviceState *dev, Error **errp)
{
    RemoteMmioState *s = REMOTE_MMIO(dev);

    if (!s->size) {
        error_setg(errp, "remote-mmio: size must be non-zero");
        return;
    }

    memory_region_init_io(&s->iomem, OBJECT(s), &remote_mmio_ops, s,
                          TYPE_REMOTE_MMIO, s->size);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

/*
 * remote_mmio_unrealize — teardown when QEMU exits or the device is destroyed.
 *
 * Sends COSIM_OP_QUIT so the SystemC side can stop cleanly.
 */
static void remote_mmio_unrealize(DeviceState *dev)
{
    RemoteMmioState *s = REMOTE_MMIO(dev);

    if (s->fd >= 0) {
        remote_mmio_transact(s, COSIM_OP_QUIT, 0, 0, NULL);
        close(s->fd);
        s->fd = -1;
    }
}

/* Device properties set from systemc_soc_init(): socket path and window size */
static const Property remote_mmio_properties[] = {
    DEFINE_PROP_STRING("socket", RemoteMmioState, socket_path),
    DEFINE_PROP_UINT64("size", RemoteMmioState, size, 0x1000),
};

/*
 * remote_mmio_class_init — register realize/unrealize hooks and properties
 * on the RemoteMmioState device class.
 */
static void remote_mmio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = remote_mmio_realize;
    dc->unrealize = remote_mmio_unrealize;
    device_class_set_props(dc, remote_mmio_properties);
}

/* QOM type registration: makes TYPE_REMOTE_MMIO available to qdev_new() */
static const TypeInfo remote_mmio_info = {
    .name = TYPE_REMOTE_MMIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RemoteMmioState),
    .instance_init = remote_mmio_instance_init,
    .class_init = remote_mmio_class_init,
};

static void remote_mmio_register_types(void)
{
    type_register_static(&remote_mmio_info);
}

type_init(remote_mmio_register_types)
