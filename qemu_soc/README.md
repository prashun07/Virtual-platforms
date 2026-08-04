# QEMU ↔ SystemC cosimulation bridge

Provides an **interface only** so your SystemC peripherals can be driven by
baremetal software running on QEMU. Integration uses **TLM-2.0 sockets**.

No peripheral models live in QEMU. The example platform binds the existing
user model at `../Timer` through `TlmPinBridge`.

## Two QEMU machines

| Machine | CPU | Use case | Run script |
|---------|-----|----------|------------|
| `systemc-soc` | Cortex-M3 | Microcontroller / baremetal M-profile | `./scripts/run_cosim.sh` |
| `systemc-ps` | Cortex-A9 + UART + GIC + DDR | Application-class PS (scalable) | `./scripts/run_cosim_ps.sh` |

## TLM topology (SystemC side)

```text
CosimServer (initiator)
    └─► TlmAddressMap (decode PL base)
            └─► TlmPinBridge (TLM target)
                    └─► Timer (user model, pin-level)
```

Set PL base with `SYSTEMC_PL_BASE`:
- M-profile (`systemc-soc`): `0x40000000`
- A-profile (`systemc-ps`): `0xF0000000`

See `include/soc_memory_map.h` for the full map.

## Build / run

```bash
./scripts/build_qemu.sh      # once — builds both machines

# Cortex-M3 cosim
./scripts/run_cosim.sh

# Cortex-A9 processing system cosim
./scripts/run_cosim_ps.sh
```

Or build pieces separately:

```bash
make -C platform             # TLM cosim executable
make -C firmware             # M-profile ELF
make -C firmware_ps          # A-profile ELF
```

## Adding peripherals

1. Native TLM model: bind `device_socket` to your `tlm_target_socket`
2. Legacy pin model (like Timer): add another `TlmPinBridge` + `map_region()`

Full architecture: [`../README.md`](../README.md)
