/*
 * systemc_ps.c
 *
 * Copyright (c) 2026 Prashun Jha. All rights reserved.
 *
 * @author Prashun Jha
 *
 * QEMU machine definition for application-class SystemC cosimulation.
 *
 * Models a minimal processing system similar to Zynq / vexpress-a9:
 *   Cortex-A9 CPU(s), GIC (via A9MPCore private peripheral block),
 *   PL011 UART console, DDR RAM, and a SystemC PL window (remote-mmio).
 *
 * M-profile workloads use -M systemc-soc (Cortex-M3) instead.
 *
 * Usage:
 *   qemu-system-arm -M systemc-ps -cpu cortex-a9 -kernel <elf>
 *
 * Memory map (see qemu_soc/include/soc_memory_map.h):
 *   0x60000000  DDR
 *   0x10009000  PL011 UART
 *   0xF0000000  PL window -> SystemC (remote-mmio)
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/boot.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "hw/char/pl011.h"
#include "hw/cpu/a9mpcore.h"
#include "hw/intc/arm_gic_common.h"
#include "exec/address-spaces.h"
#include "system/system.h"
#include "hw/misc/remote_mmio.h"
#include "target/arm/cpu-qom.h"

#define PS_SMP_CPUS          1
#define PS_GIC_EXT_IRQS      64
#define PS_DDR_SIZE_DEFAULT  (256 * 1024 * 1024)

#define PS_DDR_BASE          0x60000000ull
#define PS_A9_PERIPH_BASE    0x1e000000ull
#define PS_UART0_BASE        0x10009000ull
#define PS_PL_BASE           0xF0000000ull
#define PS_PL_SIZE           0x00100000ull

/*
 * Create Cortex-A9 CPUs, the A9MPCore private peripheral block (GIC + timers),
 * and wire CPU IRQ/FIQ/VIRQ/VFIQ lines to the interrupt controller.
 *
 * Fills pic[] with external GIC IRQ lines for UART, PL bridge, and other devices.
 */
static void ps_init_cpus(MachineState *machine, qemu_irq *pic)
{
    DeviceState *priv;
    SysBusDevice *busdev;
    int n;

    for (n = 0; n < machine->smp.cpus; n++) {
        Object *cpuobj = object_new(machine->cpu_type);

        object_property_set_bool(cpuobj, "has_el3", false, &error_fatal);
        if (object_property_find(cpuobj, "has_el2")) {
            object_property_set_bool(cpuobj, "has_el2", false, &error_fatal);
        }
        object_property_set_int(cpuobj, "reset-cbar", PS_A9_PERIPH_BASE,
                                &error_fatal);
        qdev_realize(DEVICE(cpuobj), NULL, &error_fatal);
    }

    priv = qdev_new(TYPE_A9MPCORE_PRIV);
    qdev_prop_set_uint32(priv, "num-cpu", machine->smp.cpus);
    qdev_prop_set_uint32(priv, "num-irq", PS_GIC_EXT_IRQS + GIC_INTERNAL);
    busdev = SYS_BUS_DEVICE(priv);
    sysbus_realize_and_unref(busdev, &error_fatal);
    sysbus_mmio_map(busdev, 0, PS_A9_PERIPH_BASE);

    for (n = 0; n < PS_GIC_EXT_IRQS; n++) {
        pic[n] = qdev_get_gpio_in(priv, n);
    }

    for (n = 0; n < machine->smp.cpus; n++) {
        DeviceState *cpudev = DEVICE(qemu_get_cpu(n));

        sysbus_connect_irq(busdev, n,
                           qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
        sysbus_connect_irq(busdev, n + machine->smp.cpus,
                           qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
        sysbus_connect_irq(busdev, n + 2 * machine->smp.cpus,
                           qdev_get_gpio_in(cpudev, ARM_CPU_VIRQ));
        sysbus_connect_irq(busdev, n + 3 * machine->smp.cpus,
                           qdev_get_gpio_in(cpudev, ARM_CPU_VFIQ));
    }

    sysbus_create_varargs("l2x0", PS_A9_PERIPH_BASE + 0xa000, NULL);
}

/*
 * Machine init callback; builds the A-profile processing system.
 *
 * Maps DDR, brings up CPUs + GIC, creates PL011 UART, instantiates
 * remote-mmio at PS_PL_BASE for SystemC cosim, and loads the guest kernel.
 */
static void systemc_ps_init(MachineState *machine)
{
    MemoryRegion *sysmem = get_system_memory();
    DeviceState *bridge;
    qemu_irq pic[PS_GIC_EXT_IRQS];
    const char *sock = getenv("SYSTEMC_COSIM_SOCKET");
    struct arm_boot_info bootinfo = {};

    if (!sock || !sock[0]) {
        sock = "/tmp/systemc_cosim.sock";
    }

    if (machine->ram_size > 0x40000000) {
        exit(1);
    }

    memory_region_add_subregion(sysmem, PS_DDR_BASE, machine->ram);

    ps_init_cpus(machine, pic);

    pl011_create(PS_UART0_BASE, pic[5], serial_hd(0));

    /*
     * SystemC PL window — same remote-mmio device as systemc-soc.
     * Guest accesses PS_PL_BASE; transactions forwarded to cosim_platform.
     */
    bridge = qdev_new(TYPE_REMOTE_MMIO);
    qdev_prop_set_string(bridge, "socket", sock);
    qdev_prop_set_uint64(bridge, "size", PS_PL_SIZE);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(bridge), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(bridge), 0, PS_PL_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(bridge), 0, pic[32]);
    sysbus_connect_irq(SYS_BUS_DEVICE(bridge), 1, pic[33]);

    bootinfo.ram_size = machine->ram_size;
    bootinfo.loader_start = PS_DDR_BASE;
    bootinfo.board_id = 0x8e0;
    arm_load_kernel(ARM_CPU(first_cpu), machine, &bootinfo);
}

/*
 * Register machine metadata with QEMU.
 * Sets Cortex-A9 defaults, SMP limits, and DDR size for -M systemc-ps.
 */
static void systemc_ps_machine_init(MachineClass *mc)
{
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-a9"),
        ARM_CPU_TYPE_NAME("cortex-a15"),
        NULL,
    };

    mc->desc = "SystemC PS (Cortex-A9 + UART + GIC + PL bridge)";
    mc->init = systemc_ps_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a9");
    mc->valid_cpu_types = valid_cpu_types;
    mc->default_cpus = PS_SMP_CPUS;
    mc->max_cpus = 4;
    mc->default_ram_size = PS_DDR_SIZE_DEFAULT;
    mc->default_ram_id = "systemc-ps.ddr";
    mc->ignore_memory_transaction_failures = true;
}

/* Registers -M systemc-ps in qemu-system-arm. */
DEFINE_MACHINE("systemc-ps", systemc_ps_machine_init)
