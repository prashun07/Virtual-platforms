# SystemC Model Setup Instructions

## Repository layout

```text
systemc_model/
  scripts/              # build_qemu.sh, run_platform.sh, run_cosim.sh
  platforms/            # cosim harness + firmware per target (e.g. basic_cortexM)
  Timer/                # user SystemC IP (no QEMU dependency)
  qemu_soc/             # generic bridge, wrapper, QEMU patches only
```

Models and cosim bridge are **independent**. A platform under `platforms/` connects them at run time.

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
export SYSTEMC_LIBDIR=$HOME/systemc/install/lib
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

| Piece | Role |
|-------|------|
| `qemu_soc/qemu/` | QEMU machines + `remote-mmio` bridge |
| `qemu_soc/wrapper/` | TLM + socket server |
| `platforms/basic_cortexM/` | M-profile cosim + Timer demo firmware |
| `platforms/basic_cortexA/` | A-profile cosim + Timer demo firmware |

## Step 4: Host tools (macOS)

```bash
brew install qemu arm-none-eabi-gcc ninja pkg-config glib pixman
```

Build custom QEMU once:

```bash
cd systemc_model
chmod +x scripts/*.sh
./scripts/build_qemu.sh
```

## Step 5: Run cosimulation

From **`systemc_model/`**:

```bash
./scripts/run_cosim.sh
# equivalent: ./scripts/run_platform.sh basic_cortexM
```

Expected:

```text
[cosim] listening on /tmp/systemc_cosim.sock
[platform] basic_cortexM ready; PL @ 0x40000000 ...
PASS: compare status set
ALL TESTS COMPLETED
```

### Cortex-A9 (`basic_cortexA`)

```bash
./scripts/run_cosim_ps.sh
# equivalent: ./scripts/run_platform.sh basic_cortexA
```

### Adding a new model

See [`platforms/README.md`](platforms/README.md): copy `basic_cortexM`, edit `cosim_main.cpp` and `firmware/`, add a `.env` file, run `./scripts/run_platform.sh <name>`.
