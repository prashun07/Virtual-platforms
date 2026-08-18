# QEMU ↔ SystemC cosimulation bridge

This project cosimulates **QEMU (Cortex-M3, system mode)** with a **SystemC Timer model** running in a separate process. QEMU provides the CPU, memory, and NVIC; the Timer behavior lives entirely in SystemC. QEMU only contains a generic `remote-mmio` socket bridge — no Timer logic is compiled into QEMU.

---

## Cortex-M3 example at a glance

Two processes on the host cooperate as one virtual SoC:

| Process | Binary | Role |
|---------|--------|------|
| **SystemC** | `platforms/basic_cortexM/cosim_platform` | User `Timer` model, TLM bus, Unix socket server |
| **QEMU** | `qemu-system-arm -M systemc-soc` | Cortex-M3 CPU, Flash, SRAM, NVIC, MMIO bridge |

```text
 ┌─────────────────────────────────────────────────────────────────────────┐
 │                         HOST (macOS / Linux)                            │
 │                                                                         │
 │  Process 1: QEMU (system mode)          Process 2: SystemC              │
 │  ┌────────────────────────────┐        ┌────────────────────────────┐ │
 │  │  systemc-soc               │        │  cosim_platform            │ │
 │  │                            │        │                            │ │
 │  │  Cortex-M3 (TCG)           │        │  sc_clock (10 ns)          │ │
 │  │       │                    │        │       │                    │ │
 │  │       ▼                    │        │       ▼                    │ │
 │  │  Flash / SRAM / NVIC       │        │  CosimServer (TLM init.)   │ │
 │  │       │                    │        │       │                    │ │
 │  │       ▼                    │        │       ▼                    │ │
 │  │  remote-mmio @ 0x40000000─┼─Unix───┼─► TlmAddressMap            │ │
 │  │       (no Timer logic)     │ socket │       │                    │ │
 │  │                            │        │       ▼                    │ │
 │  │                            │        │  TlmPinBridge → Timer    │ │
 │  └────────────────────────────┘        └────────────────────────────┘ │
 │                                                                         │
 │  Guest: platforms/basic_cortexM/firmware/timer_fw.elf                  │
 └─────────────────────────────────────────────────────────────────────────┘
```

**Run it:**

**Run it** (from `systemc_model/`):

```bash
./scripts/build_qemu.sh    # once
./scripts/run_cosim.sh
```

Expected output includes:

```text
[cosim] listening on /tmp/systemc_cosim.sock
[platform] TLM cosim ready; PL @ 0x40000000 ...
[cosim] QEMU connected @...
cosim: baremetal <-> QEMU <-> SystemC Timer
PASS: compare status set
PASS: overflow status set
PASS: timer stopped when disabled
ALL TESTS COMPLETED
```

---

## What QEMU owns vs what SystemC owns

| Component | QEMU (`systemc-soc`) | SystemC (`cosim_platform`) |
|-----------|----------------------|------------------------------|
| CPU | Cortex-M3 + NVIC | — |
| Flash @ `0x00000000` | Yes | — |
| SRAM @ `0x20000000` | Yes | — |
| Timer registers @ `0x40000000` | **Window only** (`remote-mmio`) | **Real model** (`Timer/`) |
| Timer counting / IRQ logic | No | Yes |
| `sc_clock` / cycle behavior | No | Yes |

Firmware treats `0x40000000` as the Timer block. It does not know SystemC exists — only that MMIO reads/writes work.

---

## Cortex-M3 memory map (`systemc-soc`)

| Region | Base | Size | Implemented in |
|--------|------|------|----------------|
| Flash (code) | `0x00000000` | 512 KiB | QEMU ROM — `timer_fw.elf` loaded here |
| SRAM | `0x20000000` | 128 KiB | QEMU RAM — stack, `.data`, `.bss` |
| **Timer (PL)** | **`0x40000000`** | 4 KiB | QEMU `remote-mmio` → socket → SystemC |
| NVIC / SysTick | `0xE0000000` | — | Standard Cortex-M private peripheral bus |

### Timer register view (firmware ↔ model)

Offsets are identical in `platforms/basic_cortexM/firmware/timer_regs.h` and `Timer/timer.h`:

| Offset | Register | Firmware macro | Meaning |
|--------|----------|----------------|---------|
| `+0x00` | Control | `TIMER_REG_CTRL` | bit0 ENABLE, bit1 CMP_EN, bit2 OV_EN |
| `+0x04` | Value | `TIMER_REG_VALUE` | Current counter |
| `+0x08` | Compare | `TIMER_REG_CMP` | Compare threshold |
| `+0x0C` | Interrupt | `TIMER_REG_INTR` | bit1 compare pending, bit2 overflow pending |

Example: `TIMER_REG_CMP` at absolute address `0x40000008`.

---

## How the two sides connect

### 1. Unix domain socket (the cosim link)

Both processes agree on a socket path (default `/tmp/systemc_cosim.sock`):

| Side | File | What it does |
|------|------|--------------|
| SystemC | `wrapper/cosim_server.cpp` | `bind()` + `listen()` — starts first |
| QEMU | `qemu/remote_mmio.c` | `connect()` on first guest MMIO access |

Environment variables (set by `run_cosim.sh`):

```bash
SYSTEMC_COSIM_SOCKET=/tmp/systemc_cosim.sock   # both processes
SYSTEMC_PL_BASE=0x40000000                     # logged on SystemC side; QEMU map is fixed in systemc_soc.c
```

### 2. SCM1 protocol (request / response)

Defined in `protocol/cosim_protocol.h`. Each guest MMIO access becomes one blocking RPC:

```text
CosimRequest  { magic='SCM1', op=READ|WRITE, addr=offset, data }
        ──────────────────────────────────────────────────────────►
CosimResponse { magic='SCM1', status=OK, data=read_value }
```

- **`addr`** = byte offset **within** the PL window (`0x00`, `0x04`, …), not the absolute CPU address `0x40000000`.
- QEMU blocks until SystemC replies — the guest CPU waits on each Timer access.

### 3. QEMU path: guest load/store → socket

```text
firmware:  STR r0, [0x40000000]     // write CTRL
    │
    ▼
QEMU CPU / memory subsystem
    │
    ▼
remote_mmio_write()                 // qemu/remote_mmio.c
    │  builds CosimRequest { WRITE, addr=0x0, data=ctrl }
    │  send() / recv() on Unix socket
    ▼
(blocks until SystemC responds)
```

QEMU machine setup is in `qemu/systemc_soc.c`:

- Instantiates `TYPE_REMOTE_MMIO` at **`0x40000000`**
- Passes socket path from `SYSTEMC_COSIM_SOCKET`
- Wires bridge IRQ lines 0/1 to NVIC (reserved for future async IRQ back-channel)
- **Does not** instantiate any Timer model

### 4. SystemC path: socket → TLM → pins → Timer

```text
CosimServer::server_thread()        // wrapper/cosim_server.cpp
    │  recv CosimRequest
    │  tlm_read() / tlm_write() via TLM initiator socket
    ▼
TlmAddressMap                       // wrapper/tlm_address_map.h
    │  decode offset (base 0 — QEMU sends window offsets)
    ▼
TlmPinBridge                        // wrapper/tlm_pin_bridge.h
    │  b_transport → toggle read_en / write_en / address / data_in
    ▼
Timer::bus_read_method / bus_write_method   // Timer/timer.h
    │  update timer_cntrl, timer_val, timer_intr, ...
    ▼
Timer::timer_thread()               // runs on sc_clock posedge
    │  increment counter, post compare/overflow to INTR
    ▼
CosimResponse returned to QEMU
```

Platform wiring is in `platforms/basic_cortexM/cosim_main.cpp`:

```text
CosimServer.initiator
    └─► TlmAddressMap.cpu_socket
            └─► TlmPinBridge.socket
                    └─► Timer (via bind_peripheral / bind_pin_bridge)
```

---

## End-to-end example: firmware enables the Timer

```text
1. run_cosim.sh starts cosim_platform
      CosimServer listens on /tmp/systemc_cosim.sock

2. run_cosim.sh starts qemu-system-arm -M systemc-soc -kernel timer_fw.elf
      Firmware boots from Flash; main() runs on emulated Cortex-M3

3. Firmware: TIMER_REG_CMP = 20
      CPU store to 0x40000008
      → remote_mmio → WRITE offset 0x8 → TlmPinBridge → Timer writes timer_cmp

4. Firmware: TIMER_REG_CTRL = ENABLE | CMP_EN | OV_EN
      CPU store to 0x40000000
      → WRITE offset 0x0 → Timer enables counting

5. CosimServer idle loop calls wait(100us) while polling socket
      → SystemC time advances → Timer sc_clock ticks
      → timer_val increments; at 20, INTR compare bit set

6. Firmware polls TIMER_REG_INTR in a loop
      Each read → READ offset 0xC → returns status from SystemC Timer
      → eventually sees TIMER_INTR_CMP → prints PASS
```

---

## Startup sequence (`run_cosim.sh`)

```text
1. make -C firmware          → timer_fw.elf
2. make -C platform          → cosim_platform (links ../../Timer/timer.h)
3. rm socket; start cosim_platform in background
4. wait until socket file exists
5. export SYSTEMC_COSIM_SOCKET
6. qemu-system-arm -M systemc-soc -cpu cortex-m3 -kernel timer_fw.elf ...
7. on exit: kill cosim_platform, remove socket
```

**Important:** Use the **custom QEMU** from `~/qemu-systemc` (built by `build_qemu.sh`). Homebrew `qemu-system-arm` does not include the `systemc-soc` machine or `remote-mmio` device.

QEMU runs in **system mode** (`qemu-system-arm`, `arm-softmmu`) — full machine emulation with MMIO. This is not user-mode Linux emulation.

---

## Key source files (Cortex-M path)

| Layer | Path | Purpose |
|-------|------|---------|
| Run script | `../scripts/run_platform.sh` | Start SystemC, then QEMU |
| QEMU machine | `qemu/systemc_soc.c` | Cortex-M3 map, bridge @ `0x40000000` |
| QEMU bridge | `qemu/remote_mmio.c` | MMIO ↔ socket RPC |
| Protocol | `protocol/cosim_protocol.h` | `CosimRequest` / `CosimResponse` |
| Socket server | `wrapper/cosim_server.cpp` | TLM initiator + `server_thread` |
| TLM bus | `wrapper/tlm_address_map.h` | Offset decode |
| Pin adapter | `wrapper/tlm_pin_bridge.h` | TLM → `read_en`/`write_en` pins |
| Pin contract | `wrapper/peripheral_if.h` | Expected Timer port names |
| Platform top | `../platforms/basic_cortexM/cosim_main.cpp` | `sc_main`, binds model + wrapper |
| **User model** | **`../Timer/`** | **Timer IP (independent of qemu_soc)** |
| Firmware | `platforms/basic_cortexM/firmware/main.c` | Baremetal tests via MMIO |
| Register defs | `platforms/basic_cortexM/firmware/timer_regs.h` | Must match Timer offsets |

---

## Design rules (why it is built this way)

1. **No peripheral logic in QEMU** — only a reusable `remote-mmio` bridge. New IP = change SystemC + firmware, not QEMU C code (unless you add more bridge windows).

2. **Timer stays pin-level** — `TlmPinBridge` adapts TLM to your existing `read_en`/`write_en` model. Native TLM peripherals can bind directly to `TlmAddressMap` later.

3. **QEMU sends window offsets** — `TlmAddressMap` maps region `0 .. WINDOW_SIZE`; absolute `0x40000000` exists only on the QEMU CPU side.

4. **Functional cosim, not cycle-accurate** — QEMU TCG is not cycle-locked to SystemC `sc_clock`. Timer advances when SystemC time moves (MMIO transactions + server idle `wait`). Correct for driver/firmware bring-up; not for hardware timing sign-off.

5. **IRQ pins reserved** — `Timer.intr1`/`intr2` drive compare/overflow status; NVIC lines are wired in `systemc_soc.c` but firmware currently **polls** `TIMER_REG_INTR`. Async SystemC → QEMU IRQ notification is planned via `CosimIrqEvent` in the protocol.

---

## Build / run reference

Run from **`systemc_model/`** (parent directory):

```bash
./scripts/build_qemu.sh      # once — installs ~/qemu-systemc/bin/qemu-system-arm

# Cortex-M3 + Timer cosim
./scripts/run_cosim.sh
# or: ./scripts/run_platform.sh basic_cortexM

# Build components separately
make -C platforms/basic_cortexM/firmware
make -C platforms/basic_cortexM MODEL_DIR=Timer
```

Platform configs: `platforms/basic_cortexM.env`, `platforms/basic_cortexA.env`. See [`platforms/README.md`](systemc_model/platforms/README.md) to add a new model.

---

## Cortex-A9 variant (`systemc-ps`)

A second machine targets application-class software (DDR, UART, GIC). The SystemC Timer stack is the same; only the QEMU machine and PL base change:

| | `systemc-soc` (M3) | `systemc-ps` (A9) |
|---|---|---|
| Run script | `../scripts/run_cosim.sh` | `../scripts/run_cosim_ps.sh` |
| PL base | `0x40000000` | `0xF0000000` |
| Firmware | `platforms/basic_cortexM/firmware/` | `platforms/basic_cortexA/firmware/` |

See `include/soc_memory_map.h` and `../scripts/run_cosim_ps.sh`.

---

## Adding more peripherals

1. **New model:** add IP under `Timer/` or a new directory; copy `platforms/_template/` and add a `platforms/<name>.env` file. Run `./scripts/run_platform.sh <name>`. See [`../platforms/README.md`](../platforms/README.md).
2. **Pin-level model (like Timer):** add another `TlmPinBridge`, extend `TlmAddressMap` with more regions in your platform `cosim_main.cpp`.
3. **Native TLM model:** bind `tlm_target_socket` directly to `TlmAddressMap::device_socket`.
4. **QEMU side:** map another `remote-mmio` instance at a new address in `systemc_soc.c` (only if you need a new MMIO window).

---

## Further reading

| Document | Content |
|----------|---------|
| [`../README.md`](../README.md) | Full virtual SoC blueprint, call chains, diagrams |
| [`../Setup.md`](../Setup.md) | Step-by-step install and first run |
| [`../Timer/Timer.md`](../Timer/Timer.md) | Timer model + senior SystemC interview Q&A |
| [`platforms/basic_cortexM/firmware/startup.md`](systemc_model/platforms/basic_cortexM/firmware/startup.md) | Cortex-M vector table and boot flow |
