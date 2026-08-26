#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MARGEN_ROOT="${MARGEN_ROOT:-${ROOT}/../margen}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
PROFILE="release"
if [[ "${BUILD_TYPE}" == "Debug" ]]; then
	PROFILE="debug"
fi

echo "Building margen_ffi (${PROFILE})..."
(
	cd "${MARGEN_ROOT}"
	if [[ "${PROFILE}" == "release" ]]; then
		cargo build -p margen_ffi --release
	else
		cargo build -p margen_ffi
	fi
)

echo "Building margen_godot (${BUILD_TYPE})..."
GENERATOR=()
if command -v ninja >/dev/null 2>&1; then
	GENERATOR=(-G Ninja)
fi
cmake -S "${ROOT}" -B "${ROOT}/build" "${GENERATOR[@]}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DMARGEN_ROOT="${MARGEN_ROOT}"
cmake --build "${ROOT}/build" --parallel

echo "Built: ${ROOT}/bin/"
