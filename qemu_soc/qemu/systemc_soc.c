/*
 * Minimal Cortex-M3 machine with a remote-mmio window for SystemC models.
 * No peripheral models are implemented here — only CPU, memory, and the bridge.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/boot.h"
#include "hw/arm/armv7m.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "exec/address-spaces.h"
#include "system/system.h"
#include "hw/misc/remote_mmio.h"

#define FLASH_SIZE   (512 * 1024)
#define SRAM_SIZE    (128 * 1024)
#define BRIDGE_BASE  0x40000000ull
#define BRIDGE_SIZE  0x1000ull

static void systemc_soc_init(MachineState *machine)
{
    DeviceState *armv7m;
    DeviceState *bridge;
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    const char *sock = getenv("SYSTEMC_COSIM_SOCKET");

    if (!sock || !sock[0]) {
        sock = "/tmp/systemc_cosim.sock";
    }

    memory_region_init_rom(flash, NULL, "systemc-soc.flash",
                           FLASH_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, 0x00000000, flash);
    memory_region_add_subregion(system_memory, 0x20000000, machine->ram);

    armv7m = qdev_new(TYPE_ARMV7M);
    qdev_prop_set_uint32(armv7m, "num-irq", 64);
    qdev_prop_set_string(armv7m, "cpu-type", machine->cpu_type);
    object_property_set_link(OBJECT(armv7m), "memory",
                             OBJECT(system_memory), &error_abort);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(armv7m), &error_fatal);

    bridge = qdev_new(TYPE_REMOTE_MMIO);
    qdev_prop_set_string(bridge, "socket", sock);
    qdev_prop_set_uint64(bridge, "size", BRIDGE_SIZE);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(bridge), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(bridge), 0, BRIDGE_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(bridge), 0, qdev_get_gpio_in(armv7m, 0));
    sysbus_connect_irq(SYS_BUS_DEVICE(bridge), 1, qdev_get_gpio_in(armv7m, 1));

    armv7m_load_kernel(ARM_CPU(first_cpu),
                       machine->kernel_filename,
                       0,
                       FLASH_SIZE);
}

static void systemc_soc_machine_init(MachineClass *mc)
{
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m3"),
        NULL,
    };

    mc->desc = "SystemC cosim SoC (Cortex-M3 + remote-mmio bridge)";
    mc->init = systemc_soc_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m3");
    mc->valid_cpu_types = valid_cpu_types;
    mc->default_ram_size = SRAM_SIZE;
    mc->default_ram_id = "systemc-soc.sram";
    mc->ignore_memory_transaction_failures = true;
}

DEFINE_MACHINE("systemc-soc", systemc_soc_machine_init)
