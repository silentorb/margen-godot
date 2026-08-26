#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MARLOTH_ROOT="${MARLOTH_ROOT:-${ROOT}/../marloth}"
ADDON_BIN="${MARLOTH_ROOT}/addons/margen/bin"

if [[ ! -d "${ROOT}/bin" ]]; then
	echo "Run ./scripts/build.sh first." >&2
	exit 1
fi

shopt -s nullglob
libs=("${ROOT}/bin/libmargen_godot."*.so)
if ((${#libs[@]} == 0)); then
	echo "No built .so found in ${ROOT}/bin — run ./scripts/build.sh" >&2
	exit 1
fi
if [[ ! -f "${ROOT}/bin/libmargen_ffi.so" ]]; then
	echo "Missing ${ROOT}/bin/libmargen_ffi.so — rebuild with ./scripts/build.sh" >&2
	exit 1
fi

mkdir -p "${ADDON_BIN}"
cp "${libs[@]}" "${ADDON_BIN}/"
cp "${ROOT}/bin/libmargen_ffi.so" "${ADDON_BIN}/"
echo "Installed ${#libs[@]} extension library(ies) + libmargen_ffi.so to ${ADDON_BIN}"
