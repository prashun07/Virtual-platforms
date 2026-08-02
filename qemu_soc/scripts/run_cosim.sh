#!/usr/bin/env bash
# Build baremetal image + SystemC cosim platform, then run QEMU against it.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${QEMU_PREFIX:-$HOME/qemu-systemc}"
QEMU_BIN="${QEMU:-${PREFIX}/bin/qemu-system-arm}"
SOCKET="${SYSTEMC_COSIM_SOCKET:-/tmp/systemc_cosim.sock}"
SYSTEMC_LIBDIR="${SYSTEMC_LIBDIR:-${SYSTEMC_HOME:-$HOME/systemc/install}/lib}"

if [[ ! -x "${QEMU_BIN}" ]]; then
  echo "error: ${QEMU_BIN} not found — run ./scripts/build_qemu.sh first"
  exit 1
fi

echo "==> Building baremetal firmware"
make -C "${ROOT}/firmware"

echo "==> Building SystemC cosim platform (user Timer + wrapper)"
make -C "${ROOT}/platform"

rm -f "${SOCKET}"

echo "==> Starting SystemC side on ${SOCKET}"
DYLD_LIBRARY_PATH="${SYSTEMC_LIBDIR}:${DYLD_LIBRARY_PATH:-}" \
  "${ROOT}/platform/cosim_platform" "${SOCKET}" &
SC_PID=$!

cleanup() {
  kill "${SC_PID}" 2>/dev/null || true
  wait "${SC_PID}" 2>/dev/null || true
  rm -f "${SOCKET}"
}
trap cleanup EXIT

# Wait until the Unix socket exists
for _ in $(seq 1 50); do
  if [[ -S "${SOCKET}" ]]; then
    break
  fi
  sleep 0.1
done
if [[ ! -S "${SOCKET}" ]]; then
  echo "error: SystemC cosim socket did not appear"
  exit 1
fi

echo "==> Starting QEMU Cortex-M3 (remote-mmio -> SystemC)"
export SYSTEMC_COSIM_SOCKET="${SOCKET}"
"${QEMU_BIN}" \
  -M systemc-soc \
  -cpu cortex-m3 \
  -kernel "${ROOT}/firmware/timer_fw.elf" \
  -semihosting-config enable=on,target=native \
  -nographic
