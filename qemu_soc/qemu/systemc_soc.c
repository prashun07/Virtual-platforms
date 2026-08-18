/*
 * systemc_soc.c
 *
 * Copyright (c) 2026 Prashun Jha. All rights reserved.
 *
 * @author Prashun Jha
 *
 * QEMU machine definition for Cortex-M3 SystemC cosimulation.
 *
 * Defines the "systemc-soc" virtual SoC: Cortex-M3 + Flash + SRAM + remote-mmio
 * bridge at 0x40000000. Peripheral behavior (Timer) runs in a separate SystemC
 * process; this file only builds the CPU-side address map and wires the bridge.
 *
 * Usage:
 *   qemu-system-arm -M systemc-soc -cpu cortex-m3 -kernel timer_fw.elf
 *
 * Memory map:
 *   0x00000000  Flash (512 KiB) — baremetal firmware
 *   0x20000000  SRAM (128 KiB)  — stack, .data, .bss
 *   0x40000000  PL window       — remote-mmio -> SystemC Timer
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/boot.h"
#include "hw/arm/armv7m.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "hw/qdev-clock.h"
#include "exec/address-spaces.h"
#include "system/system.h"
#include "hw/misc/remote_mmio.h"

#define FLASH_SIZE   (512 * 1024)
#define SRAM_SIZE    (128 * 1024)
#define BRIDGE_BASE  0x40000000ull
#define BRIDGE_SIZE  0x1000ull
#define HCLK_FRQ     25000000ULL

/*
 * Machine init callback; builds the virtual M-profile SoC.
 *
 * Creates Flash, SRAM, ARMv7M (Cortex-M3 + NVIC), and remote-mmio at
 * BRIDGE_BASE. Does NOT instantiate any Timer model — that lives in
 * cosim_platform on the SystemC side.
 */
static void systemc_soc_init(MachineState *machine)
{
    DeviceState *armv7m;
    DeviceState *bridge;
    Clock *sysclk;
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

    /* QEMU 10+ requires armv7m cpuclk before realize. */
    sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(sysclk, HCLK_FRQ);

    armv7m = qdev_new(TYPE_ARMV7M);
    qdev_prop_set_uint32(armv7m, "num-irq", 64);
    qdev_prop_set_string(armv7m, "cpu-type", machine->cpu_type);
    qdev_connect_clock_in(armv7m, "cpuclk", sysclk);
    object_property_set_link(OBJECT(armv7m), "memory",
                             OBJECT(system_memory), &error_abort);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(armv7m), &error_fatal);

    /*
     * remote-mmio bridge: guest MMIO at BRIDGE_BASE -> CosimServer socket.
     * Socket path must match CosimServer (see run_cosim.sh).
     */
    bridge = qdev_new(TYPE_REMOTE_MMIO);
    qdev_prop_set_string(bridge, "socket", sock);
    qdev_prop_set_uint64(bridge, "size", BRIDGE_SIZE);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(bridge), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(bridge), 0, BRIDGE_BASE);

    /* IRQ wiring reserved: bridge irq[0]/[1] -> NVIC 0/1 (async path TBD). */
    sysbus_connect_irq(SYS_BUS_DEVICE(bridge), 0, qdev_get_gpio_in(armv7m, 0));
    sysbus_connect_irq(SYS_BUS_DEVICE(bridge), 1, qdev_get_gpio_in(armv7m, 1));

    /*
     * Load baremetal ELF into Flash and set PC/SP from the vector table.
     * After this, firmware Reset_Handler runs on the emulated Cortex-M3.
     */
    armv7m_load_kernel(ARM_CPU(first_cpu),
                       machine->kernel_filename,
                       0,
                       FLASH_SIZE);
}

/*
 * Register machine metadata with QEMU.
 * Hooks systemc_soc_init as the board init and sets CPU/RAM defaults.
 */
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

/* Registers -M systemc-soc in qemu-system-arm. */
DEFINE_MACHINE("systemc-soc", systemc_soc_machine_init)
