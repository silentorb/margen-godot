#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MARGEN_ROOT="${MARGEN_ROOT:-${ROOT}/../margen}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
TARGET="${TARGET:-linux}"
PROFILE="release"
if [[ "${BUILD_TYPE}" == "Debug" ]]; then
	PROFILE="debug"
fi

GENERATOR=()
if command -v ninja >/dev/null 2>&1; then
	GENERATOR=(-G Ninja)
fi

case "${TARGET}" in
	linux)
		echo "Building margen_ffi (${PROFILE}, host)..."
		(
			cd "${MARGEN_ROOT}"
			if [[ "${PROFILE}" == "release" ]]; then
				cargo build -p margen_ffi --release
			else
				cargo build -p margen_ffi
			fi
		)

		echo "Building margen_godot (${BUILD_TYPE}, linux)..."
		cmake -S "${ROOT}" -B "${ROOT}/build" "${GENERATOR[@]}" \
			-DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
			-DMARGEN_ROOT="${MARGEN_ROOT}"
		cmake --build "${ROOT}/build" --parallel
		;;
	windows)
		TRIPLE="x86_64-pc-windows-msvc"
		TOOLCHAIN="${ROOT}/cmake/clang-cl-xwin-x86_64.cmake"
		if [[ ! -f "${TOOLCHAIN}" ]]; then
			echo "Missing clang-cl/xwin toolchain file: ${TOOLCHAIN}" >&2
			exit 1
		fi
		if ! command -v cargo-xwin >/dev/null 2>&1 && ! cargo xwin --help >/dev/null 2>&1; then
			echo "cargo-xwin is required for TARGET=windows." >&2
			echo "Rebuild the marloth-win image (cargo-xwin + xwin CRT/SDK are image-only)." >&2
			exit 1
		fi
		if [[ -z "${XWIN_CACHE_DIR:-}" ]]; then
			export XWIN_CACHE_DIR="/opt/cargo-xwin"
		fi
		if [[ -d "${XWIN_CACHE_DIR}/xwin/crt" && -d "${XWIN_CACHE_DIR}/xwin/sdk" ]]; then
			:
		elif [[ -d "${XWIN_CACHE_DIR}/crt" && -d "${XWIN_CACHE_DIR}/sdk" ]]; then
			:
		else
			echo "Xwin CRT/SDK missing under ${XWIN_CACHE_DIR} (expected ${XWIN_CACHE_DIR}/xwin/crt)." >&2
			echo "Rebuild the marloth-win image — do not download CRT/SDK at build time." >&2
			exit 1
		fi

		echo "Building margen_ffi (${PROFILE}, ${TRIPLE} via cargo-xwin)..."
		if [[ -n "${CARGO_TARGET_DIR:-}" ]]; then
			echo "CARGO_TARGET_DIR=${CARGO_TARGET_DIR}"
			mkdir -p "${CARGO_TARGET_DIR}"
		fi
		(
			cd "${MARGEN_ROOT}"
			if [[ "${PROFILE}" == "release" ]]; then
				cargo xwin build -p margen_ffi --release --target "${TRIPLE}"
			else
				cargo xwin build -p margen_ffi --target "${TRIPLE}"
			fi
		)

		echo "Building margen_godot (${BUILD_TYPE}, windows/clang-cl+xwin)..."
		# Wipe stale MinGW/MSVC CMake cache when switching toolchains.
		if [[ -f "${ROOT}/build-windows/CMakeCache.txt" ]]; then
			prev_tc=$(grep -E '^CMAKE_TOOLCHAIN_FILE:' "${ROOT}/build-windows/CMakeCache.txt" 2>/dev/null | cut -d= -f2- || true)
			if [[ "${prev_tc}" != "${TOOLCHAIN}" ]]; then
				echo "Toolchain changed; clearing ${ROOT}/build-windows ..."
				if ! rm -rf "${ROOT}/build-windows"; then
					echo "Failed to clear ${ROOT}/build-windows (permissions?). Remove it and retry." >&2
					exit 1
				fi
			fi
		fi
		# Root-owned leftovers from prior agent runs block host/vscode cleanup.
		if [[ -d "${ROOT}/build-windows" ]] && [[ ! -w "${ROOT}/build-windows" ]]; then
			echo "Clearing unwritable ${ROOT}/build-windows ..."
			rm -rf "${ROOT}/build-windows"
		fi
		cmake -S "${ROOT}" -B "${ROOT}/build-windows" "${GENERATOR[@]}" \
			-DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
			-DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
			-DMARGEN_ROOT="${MARGEN_ROOT}" \
			-DMARGEN_FFI_TARGET="${TRIPLE}"
		cmake --build "${ROOT}/build-windows" --parallel
		;;
	*)
		echo "Unknown TARGET=${TARGET} (expected linux or windows)" >&2
		exit 1
		;;
esac

echo "Built: ${ROOT}/bin/"
