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
		TRIPLE="x86_64-pc-windows-gnu"
		TOOLCHAIN="${ROOT}/cmake/mingw-w64-x86_64.cmake"
		if [[ ! -f "${TOOLCHAIN}" ]]; then
			echo "Missing MinGW toolchain file: ${TOOLCHAIN}" >&2
			exit 1
		fi

		echo "Building margen_ffi (${PROFILE}, ${TRIPLE})..."
		(
			cd "${MARGEN_ROOT}"
			if [[ "${PROFILE}" == "release" ]]; then
				cargo build -p margen_ffi --release --target "${TRIPLE}"
			else
				cargo build -p margen_ffi --target "${TRIPLE}"
			fi
		)

		echo "Building margen_godot (${BUILD_TYPE}, windows/MinGW)..."
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
