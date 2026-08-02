# QEMU ↔ SystemC cosimulation bridge

Provides an **interface only** so your SystemC peripherals can be driven by
baremetal software running on a QEMU ARM Cortex-M3.

No peripheral models live in QEMU. The example platform binds the existing
user model at `../Timer`.

```text
firmware (ARM) → QEMU remote-mmio → socket → wrapper → your SystemC model
```

## Build / run

```bash
./scripts/build_qemu.sh   # once: QEMU with remote-mmio bridge
./scripts/run_cosim.sh    # baremetal + SystemC Timer cosim
```

Or build pieces separately:

```bash
make -C firmware          # baremetal ELF
make -C platform          # SystemC cosim executable
```

## Architecture blueprint

The detailed SoC architecture (CPU, memory map, Timer↔QEMU data path,
cosim protocol, boot sequence) is documented in the project root:

**[`../README.md` — Virtual SoC Simulation Blueprint (QEMU + SystemC)](../README.md#virtual-soc-simulation-blueprint-qemu--systemc)**

Also see [`../Setup.md`](../Setup.md) for prerequisites and how to plug in a new model.
