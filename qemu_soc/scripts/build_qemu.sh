#!/usr/bin/env bash
# Build local QEMU (arm-softmmu) with the generic remote-mmio bridge only.
# No SystemC peripheral models are compiled into QEMU.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QEMU_VERSION="${QEMU_VERSION:-v10.0.0}"
SRC_DIR="${QEMU_SRC_DIR:-$HOME/qemu-systemc-src}"
BUILD_DIR="${QEMU_BUILD_DIR:-$HOME/qemu-systemc-build}"
PREFIX="${QEMU_PREFIX:-$HOME/qemu-systemc}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

# Use Apple toolchain (Homebrew LLVM can break QEMU configure on macOS).
export CC="${CC:-/usr/bin/cc}"
export CXX="${CXX:-/usr/bin/c++}"
export OBJC="${OBJC:-/usr/bin/cc}"
export AR="${AR:-/usr/bin/ar}"
export SDKROOT="${SDKROOT:-$(xcrun --show-sdk-path 2>/dev/null || true)}"
export PYTHON="${PYTHON:-/opt/homebrew/bin/python3}"
# Keep Homebrew LLVM off PATH so Meson does not pick it for Objective-C.
export PATH="$(printf '%s\n' "$PATH" | tr ':' '\n' | grep -v '/opt/homebrew/opt/llvm' | paste -sd: -)"

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "error: missing '$1' — install with: brew install $2"
    exit 1
  fi
}

need_cmd ninja ninja
need_cmd pkg-config pkg-config

echo "==> QEMU ${QEMU_VERSION} (remote-mmio bridge) -> ${PREFIX}"
echo "    CC=${CC} OBJC=${OBJC}  SDKROOT=${SDKROOT:-<unset>}"

if [[ ! -d "${SRC_DIR}/.git" ]]; then
  git clone --depth 1 --branch "${QEMU_VERSION}" \
    https://gitlab.com/qemu-project/qemu.git "${SRC_DIR}"
  git -C "${SRC_DIR}" submodule update --init --recursive
fi

mkdir -p "${SRC_DIR}/include/hw/misc"
cp "${ROOT}/qemu/remote_mmio.h"              "${SRC_DIR}/include/hw/misc/remote_mmio.h"
cp "${ROOT}/protocol/cosim_protocol.h"      "${SRC_DIR}/include/hw/misc/cosim_protocol.h"
cp "${ROOT}/qemu/remote_mmio.c"             "${SRC_DIR}/hw/misc/remote_mmio.c"
cp "${ROOT}/qemu/systemc_soc.c"             "${SRC_DIR}/hw/arm/systemc_soc.c"
cp "${ROOT}/qemu/systemc_ps.c"             "${SRC_DIR}/hw/arm/systemc_ps.c"

if ! grep -q 'REMOTE_MMIO' "${SRC_DIR}/hw/misc/Kconfig"; then
  cat >> "${SRC_DIR}/hw/misc/Kconfig" <<'EOF'

config REMOTE_MMIO
    bool
EOF
fi

if ! grep -q 'SYSTEMC_SOC' "${SRC_DIR}/hw/arm/Kconfig"; then
  cat >> "${SRC_DIR}/hw/arm/Kconfig" <<'EOF'

config SYSTEMC_SOC
    bool
    default y
    depends on TCG && ARM
    select ARM_V7M
    select REMOTE_MMIO
EOF
fi

if ! grep -q 'remote_mmio.c' "${SRC_DIR}/hw/misc/meson.build"; then
  cat >> "${SRC_DIR}/hw/misc/meson.build" <<'EOF'

system_ss.add(when: 'CONFIG_REMOTE_MMIO', if_true: files('remote_mmio.c'))
EOF
fi

if ! grep -q 'systemc_soc.c' "${SRC_DIR}/hw/arm/meson.build"; then
  cat >> "${SRC_DIR}/hw/arm/meson.build" <<'EOF'

system_ss.add(when: 'CONFIG_SYSTEMC_SOC', if_true: files('systemc_soc.c'))
EOF
fi

if ! grep -q 'SYSTEMC_PS' "${SRC_DIR}/hw/arm/Kconfig"; then
  cat >> "${SRC_DIR}/hw/arm/Kconfig" <<'EOF'

config SYSTEMC_PS
    bool
    default y
    depends on TCG && ARM
    select A9MPCORE
    select PL011
    select REMOTE_MMIO
EOF
fi

if ! grep -q 'systemc_ps.c' "${SRC_DIR}/hw/arm/meson.build"; then
  cat >> "${SRC_DIR}/hw/arm/meson.build" <<'EOF'

system_ss.add(when: 'CONFIG_SYSTEMC_PS', if_true: files('systemc_ps.c'))
EOF
fi

# Refresh sources if the tree already existed from an older setup
cp "${ROOT}/qemu/remote_mmio.h"         "${SRC_DIR}/include/hw/misc/remote_mmio.h"
cp "${ROOT}/protocol/cosim_protocol.h" "${SRC_DIR}/include/hw/misc/cosim_protocol.h"
cp "${ROOT}/qemu/remote_mmio.c"        "${SRC_DIR}/hw/misc/remote_mmio.c"
cp "${ROOT}/qemu/systemc_soc.c"        "${SRC_DIR}/hw/arm/systemc_soc.c"
cp "${ROOT}/qemu/systemc_ps.c"        "${SRC_DIR}/hw/arm/systemc_ps.c"

# Drop any previously injected timer model files
rm -f "${SRC_DIR}/hw/misc/systemc_timer.c" \
      "${SRC_DIR}/include/hw/misc/systemc_timer.h"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

if [[ ! -f build.ninja ]]; then
  rm -rf "${BUILD_DIR:?}/"*
  "${SRC_DIR}/configure" \
    --prefix="${PREFIX}" \
    --target-list=arm-softmmu \
    --disable-docs \
    --disable-user \
    --enable-tcg
fi

ninja -j"${JOBS}"
ninja install

echo
echo "Installed: ${PREFIX}/bin/qemu-system-arm"
echo "Check:"
echo "  ${PREFIX}/bin/qemu-system-arm -machine help | grep systemc"
