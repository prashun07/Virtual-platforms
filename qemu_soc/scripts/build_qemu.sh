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

echo "==> QEMU ${QEMU_VERSION} (remote-mmio bridge) -> ${PREFIX}"

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

# Refresh sources if the tree already existed from an older setup
cp "${ROOT}/qemu/remote_mmio.h"         "${SRC_DIR}/include/hw/misc/remote_mmio.h"
cp "${ROOT}/protocol/cosim_protocol.h" "${SRC_DIR}/include/hw/misc/cosim_protocol.h"
cp "${ROOT}/qemu/remote_mmio.c"        "${SRC_DIR}/hw/misc/remote_mmio.c"
cp "${ROOT}/qemu/systemc_soc.c"        "${SRC_DIR}/hw/arm/systemc_soc.c"

# Drop any previously injected timer model files
rm -f "${SRC_DIR}/hw/misc/systemc_timer.c" \
      "${SRC_DIR}/include/hw/misc/systemc_timer.h"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

if [[ ! -f build.ninja ]]; then
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
echo "Check: ${PREFIX}/bin/qemu-system-arm -machine help | grep systemc-soc"
