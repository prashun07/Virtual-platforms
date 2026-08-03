/*
 * systemc_soc.c — QEMU machine definition for SystemC cosimulation.
 *
 * Defines the "systemc-soc" virtual chip: Cortex-M3 + Flash + SRAM + remote-mmio
 * bridge. Peripheral behavior (Timer) runs in a separate SystemC process; this
 * file only builds the CPU-side address map and wires the bridge at 0x40000000.
 *
 * Select with: qemu-system-arm -M systemc-soc -cpu cortex-m3 -kernel <elf>
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

#define FLASH_SIZE   (512 * 1024)   /* code region for baremetal firmware */
#define SRAM_SIZE    (128 * 1024)   /* stack, .data, .bss */
#define BRIDGE_BASE  0x40000000ull  /* guest sees Timer registers here */
#define BRIDGE_SIZE  0x1000ull      /* 4 KiB MMIO window -> remote-mmio */

/*
 * systemc_soc_init — machine init callback; builds the virtual SoC.
 *
 * Called once when QEMU starts with -M systemc-soc. Creates:
 *   - Flash ROM at 0x00000000 (firmware loaded by armv7m_load_kernel)
 *   - SRAM at 0x20000000
 *   - ARMv7M container (Cortex-M3 CPU + NVIC)
 *   - remote-mmio device at 0x40000000 (socket bridge to SystemC)
 *
 * Does NOT instantiate any Timer model — that lives in cosim_platform.
 */
static void systemc_soc_init(MachineState *machine)
{
    DeviceState *armv7m;
    DeviceState *bridge;
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    const char *sock = getenv("SYSTEMC_COSIM_SOCKET");

    /* Must match the socket CosimServer listens on (see run_cosim.sh) */
    if (!sock || !sock[0]) {
        sock = "/tmp/systemc_cosim.sock";
    }

    /* --- Memory map: Flash (code) --- */
    memory_region_init_rom(flash, NULL, "systemc-soc.flash",
                           FLASH_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, 0x00000000, flash);

    /* --- Memory map: SRAM (data/stack); size from -m or default_ram_size --- */
    memory_region_add_subregion(system_memory, 0x20000000, machine->ram);

    /*
     * --- CPU: ARMv7-M (Cortex-M3) + NVIC ---
     * TYPE_ARMV7M wraps the CPU and interrupt controller. Guest firmware
     * vector table at 0x00000000 is handled after kernel load below.
     */
    armv7m = qdev_new(TYPE_ARMV7M);
    qdev_prop_set_uint32(armv7m, "num-irq", 64);
    qdev_prop_set_string(armv7m, "cpu-type", machine->cpu_type);
    object_property_set_link(OBJECT(armv7m), "memory",
                             OBJECT(system_memory), &error_abort);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(armv7m), &error_fatal);

    /*
     * --- Bridge: remote-mmio -> SystemC cosim socket ---
     * Guest load/store to BRIDGE_BASE is forwarded to CosimServer, which
     * calls MmioAdapter and ultimately the user Timer model.
     */
    bridge = qdev_new(TYPE_REMOTE_MMIO);
    qdev_prop_set_string(bridge, "socket", sock);
    qdev_prop_set_uint64(bridge, "size", BRIDGE_SIZE);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(bridge), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(bridge), 0, BRIDGE_BASE);

    /*
     * IRQ wiring (reserved): bridge irq[0]/[1] -> NVIC lines 0/1.
     * Firmware currently polls TIMER_REG_INTR; async IRQ back-channel TBD.
     */
    sysbus_connect_irq(SYS_BUS_DEVICE(bridge), 0, qdev_get_gpio_in(armv7m, 0));
    sysbus_connect_irq(SYS_BUS_DEVICE(bridge), 1, qdev_get_gpio_in(armv7m, 1));

    /*
     * Load baremetal ELF (-kernel timer_fw.elf) into Flash and set CPU PC/SP
     * from the vector table. After this, Reset_Handler in firmware runs.
     */
    armv7m_load_kernel(ARM_CPU(first_cpu),
                       machine->kernel_filename,
                       0,
                       FLASH_SIZE);
}

/*
 * systemc_soc_machine_init — register machine metadata with QEMU.
 *
 * Hooks systemc_soc_init as the board init function and sets defaults
 * (CPU type, RAM size). Invoked via DEFINE_MACHINE at file bottom.
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

/* Registers the machine; makes -M systemc-soc available in qemu-system-arm */
DEFINE_MACHINE("systemc-soc", systemc_soc_machine_init)
