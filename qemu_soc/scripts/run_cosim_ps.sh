#!/usr/bin/env bash
exec "$(cd "$(dirname "$0")/../.." && pwd)/scripts/run_cosim_ps.sh" "$@"
