#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MARLOTH_ROOT="${MARLOTH_ROOT:-${ROOT}/../marloth}"
ADDON_BIN="${MARLOTH_ROOT}/addons/margen/bin"
# linux | windows | all (default: install whatever is present)
PLATFORM="${PLATFORM:-all}"

if [[ ! -d "${ROOT}/bin" ]]; then
	echo "Run ./scripts/build.sh first." >&2
	exit 1
fi

shopt -s nullglob
libs_so=("${ROOT}/bin/libmargen_godot."*.so)
libs_dll=("${ROOT}/bin/libmargen_godot."*.dll)

mkdir -p "${ADDON_BIN}"
installed=0

if [[ "${PLATFORM}" == "linux" || "${PLATFORM}" == "all" ]]; then
	if ((${#libs_so[@]} > 0)); then
		if [[ ! -f "${ROOT}/bin/libmargen_ffi.so" ]]; then
			echo "Missing ${ROOT}/bin/libmargen_ffi.so — rebuild with TARGET=linux ./scripts/build.sh" >&2
			exit 1
		fi
		cp "${libs_so[@]}" "${ADDON_BIN}/"
		cp "${ROOT}/bin/libmargen_ffi.so" "${ADDON_BIN}/"
		echo "Installed ${#libs_so[@]} Linux extension library(ies) + libmargen_ffi.so to ${ADDON_BIN}"
		installed=1
	elif [[ "${PLATFORM}" == "linux" ]]; then
		echo "No built libmargen_godot.*.so in ${ROOT}/bin — run TARGET=linux ./scripts/build.sh" >&2
		exit 1
	fi
fi

if [[ "${PLATFORM}" == "windows" || "${PLATFORM}" == "all" ]]; then
	if ((${#libs_dll[@]} > 0)); then
		if [[ ! -f "${ROOT}/bin/margen_ffi.dll" ]]; then
			echo "Missing ${ROOT}/bin/margen_ffi.dll — rebuild with TARGET=windows ./scripts/build.sh" >&2
			exit 1
		fi
		cp "${libs_dll[@]}" "${ADDON_BIN}/"
		cp "${ROOT}/bin/margen_ffi.dll" "${ADDON_BIN}/"
		echo "Installed ${#libs_dll[@]} Windows extension library(ies) + margen_ffi.dll to ${ADDON_BIN}"
		installed=1
	elif [[ "${PLATFORM}" == "windows" ]]; then
		echo "No built libmargen_godot.*.dll in ${ROOT}/bin — run TARGET=windows ./scripts/build.sh" >&2
		exit 1
	fi
fi

if ((installed == 0)); then
	echo "No built libmargen_godot.* found in ${ROOT}/bin — run ./scripts/build.sh" >&2
	exit 1
fi
