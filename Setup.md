# SystemC Model Setup Instructions

## Step 1: Build SystemC from source

```bash
mkdir -p ~/systemc && cd ~/systemc
curl -L -o systemc-2.3.4.tar.gz \
  https://github.com/accellera-official/systemc/archive/refs/tags/2.3.4.tar.gz
tar -xzf systemc-2.3.4.tar.gz && cd systemc-2.3.4
mkdir build && cd build
cmake .. -DCMAKE_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX=$HOME/systemc/install
make -j$(sysctl -n hw.ncpu) && make install
```

## Step 2: Set env vars (add to ~/.zshrc)

```bash
export SYSTEMC_HOME=$HOME/systemc/install
export SYSTEMC_INCLUDE=$SYSTEMC_HOME/include
export SYSTEMC_LIBDIR=$SYSTEMC_HOME/lib
export QEMU_PREFIX=$HOME/qemu-systemc
export PATH="$QEMU_PREFIX/bin:$PATH"
```

```bash
source ~/.zshrc
```

## Step 3: Standalone SystemC timer testbench (no QEMU)

```bash
cd Timer
/usr/bin/clang++ -std=c++17 -I"$SYSTEMC_INCLUDE" timer_tb.cpp \
  -L"$SYSTEMC_LIBDIR" -lsystemc -o timer_sim
DYLD_LIBRARY_PATH="$SYSTEMC_LIBDIR" ./timer_sim
```

---

# QEMU + SystemC cosimulation

Architecture (your model stays in SystemC; QEMU only provides CPU + bridge):

```text
  baremetal ELF
       |
  QEMU Cortex-M3  --remote-mmio-->  Unix socket  -->  SystemC wrapper
                                                         |
                                                    user model (Timer)
```

| Piece | Role |
|-------|------|
| `qemu/remote_mmio.*` | Generic MMIO↔socket bridge (no model logic) |
| `qemu/systemc_soc.c` | Cortex-M3 machine, PL bridge @ `0x40000000` |
| `qemu/systemc_ps.c` | Cortex-A9 PS (UART, GIC, DDR), PL @ `0xF0000000` |
| `wrapper/` | TLM sockets + `TlmPinBridge` to user pin-level models |
| `platform/cosim_main.cpp` | CosimServer → TlmAddressMap → Timer |
| `firmware/` | M-profile baremetal |
| `firmware_ps/` | A-profile baremetal |

### TLM integration

SystemC uses **TLM-2.0** (`CosimServer` initiator → `TlmAddressMap` →
`TlmPinBridge` → your model). Pin-level models like `Timer` stay unchanged;
native TLM peripherals can bind directly to the address map later.

## Step 4: Host tools (macOS)

```bash
brew install qemu arm-none-eabi-gcc ninja pkg-config glib pixman
```

Homebrew QEMU does not include the bridge. Build the local QEMU once:

```bash
cd qemu_soc
chmod +x scripts/*.sh
./scripts/build_qemu.sh
```

## Step 5: Build the baremetal image

```bash
cd qemu_soc/firmware
make
# produces timer_fw.elf / timer_fw.bin
```

## Step 6: Run cosimulation

```bash
cd qemu_soc
./scripts/run_cosim.sh
```

This will:

1. Build firmware + SystemC platform (links `Timer/timer.h` as-is)
2. Start the SystemC side (listens on `/tmp/systemc_cosim.sock`)
3. Start QEMU `systemc-soc` with the baremetal ELF

Expected:

```text
[cosim] listening on /tmp/systemc_cosim.sock
cosim: baremetal <-> QEMU <-> SystemC Timer
PASS: compare status set
PASS: overflow status set
PASS: timer stopped when disabled
ALL TESTS COMPLETED
```

### Memory map (Cortex-M3, `systemc-soc`)

| Address    | Region |
| ---------- | ------ |
| 0x00000000 | Flash (baremetal) |
| 0x20000000 | SRAM |
| 0x40000000 | PL window → SystemC TLM (Timer) |

### Cortex-A9 processing system (`systemc-ps`)

```bash
cd qemu_soc
./scripts/run_cosim_ps.sh
```

| Address    | Region |
| ---------- | ------ |
| 0x60000000 | DDR |
| 0x10009000 | PL011 UART |
| 0xF0000000 | PL window → SystemC TLM (Timer) |

### Connecting a new custom model later

1. **Pin-level** (like Timer): add `TlmPinBridge`, `map_region()`, bind in `cosim_main.cpp`
2. **Native TLM**: bind your `tlm_target_socket` to `TlmAddressMap::device_socket` (or add a second region)
3. See `wrapper/peripheral_if.h` for the legacy pin contract
4. Rebuild: `make -C qemu_soc/platform`
5. Run `./scripts/run_cosim.sh` (M3) or `./scripts/run_cosim_ps.sh` (A9)
