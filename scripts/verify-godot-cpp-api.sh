#!/usr/bin/env bash
# Audit godot-cpp extension_api.json / submodule alignment vs a target Godot version.
# Does not require a Godot process. Exit 0 always unless --strict and a FAIL is raised.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
API_JSON="${ROOT}/godot-cpp/gdextension/extension_api.json"
INTERFACE_H="${ROOT}/godot-cpp/gdextension/gdextension_interface.h"
GITMODULES="${ROOT}/.gitmodules"
TARGET_VERSION="${GODOT_VERSION:-4.6.1}"
STRICT=0
JSON_OUT=0

usage() {
	echo "Usage: $0 [--strict] [--json] [GODOT_VERSION]" >&2
	echo "  GODOT_VERSION defaults to \$GODOT_VERSION or 4.6.1" >&2
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		--strict) STRICT=1; shift ;;
		--json) JSON_OUT=1; shift ;;
		-h|--help) usage; exit 0 ;;
		*) TARGET_VERSION="$1"; shift ;;
	esac
done

pass=0
warn=0
fail=0
lines=()

report() {
	local level="$1"
	local msg="$2"
	lines+=("${level}: ${msg}")
	case "${level}" in
		PASS) pass=$((pass + 1)) ;;
		WARN) warn=$((warn + 1)) ;;
		FAIL) fail=$((fail + 1)) ;;
	esac
}

if [[ ! -f "${API_JSON}" ]]; then
	report FAIL "missing ${API_JSON}"
else
	api_major=$(grep -o '"version_major"[[:space:]]*:[[:space:]]*[0-9]*' "${API_JSON}" | head -1 | grep -o '[0-9]*$')
	api_minor=$(grep -o '"version_minor"[[:space:]]*:[[:space:]]*[0-9]*' "${API_JSON}" | head -1 | grep -o '[0-9]*$')
	api_patch=$(grep -o '"version_patch"[[:space:]]*:[[:space:]]*[0-9]*' "${API_JSON}" | head -1 | grep -o '[0-9]*$')
	api_ver="${api_major}.${api_minor}.${api_patch}"
	report PASS "extension_api.json header version=${api_ver}"

	target_major="${TARGET_VERSION%%.*}"
	rest="${TARGET_VERSION#*.}"
	target_minor="${rest%%.*}"
	if [[ "${api_major}" != "${target_major}" ]]; then
		report FAIL "API major ${api_major} != target major ${target_major} (${TARGET_VERSION})"
	elif [[ "${api_minor}" -lt "${target_minor}" ]]; then
		report WARN "API ${api_ver} is older than target Godot ${TARGET_VERSION} (gdextension_interface may also lag)"
	elif [[ "${api_minor}" -gt "${target_minor}" ]]; then
		report WARN "API ${api_ver} is newer than target Godot ${TARGET_VERSION}"
	else
		report PASS "API minor matches target Godot ${TARGET_VERSION}"
	fi
fi

if [[ ! -f "${INTERFACE_H}" ]]; then
	report FAIL "missing ${INTERFACE_H}"
else
	report PASS "gdextension_interface.h present"
	if [[ -f "${API_JSON}" ]]; then
		# Flag mismatched update times as a hint that JSON was swapped without the header.
		api_mtime=$(stat -c %Y "${API_JSON}" 2>/dev/null || stat -f %m "${API_JSON}")
		hdr_mtime=$(stat -c %Y "${INTERFACE_H}" 2>/dev/null || stat -f %m "${INTERFACE_H}")
		delta=$((api_mtime - hdr_mtime))
		if ((delta < 0)); then
			delta=$((-delta))
		fi
		# More than ~1 day apart after a partial API refresh is a yellow flag.
		if ((delta > 86400)); then
			report WARN "extension_api.json and gdextension_interface.h mtimes differ by ${delta}s — possible partial API update"
		else
			report PASS "extension_api.json and gdextension_interface.h mtimes are close (${delta}s)"
		fi
	fi
fi

if [[ -f "${GITMODULES}" ]]; then
	branch=$(awk '/\[submodule "godot-cpp"\]/{f=1} f && /branch =/{print $3; exit}' "${GITMODULES}" || true)
	if [[ -z "${branch}" ]]; then
		report WARN "godot-cpp submodule branch not set in .gitmodules"
	else
		report PASS "godot-cpp submodule branch=${branch}"
		if [[ "${branch}" != "${target_major}.${target_minor}" && "${branch}" != "${TARGET_VERSION}" ]]; then
			report WARN "godot-cpp branch '${branch}' does not match target Godot ${TARGET_VERSION}"
		fi
	fi
else
	report WARN "missing .gitmodules"
fi

if [[ "${JSON_OUT}" -eq 1 ]]; then
	printf '{'
	printf '"target_version":"%s",' "${TARGET_VERSION}"
	printf '"pass":%s,' "${pass}"
	printf '"warn":%s,' "${warn}"
	printf '"fail":%s,' "${fail}"
	printf '"lines":['
	first=1
	for line in "${lines[@]}"; do
		if [[ ${first} -eq 1 ]]; then first=0; else printf ','; fi
		# Escape quotes for minimal JSON.
		esc=${line//\\/\\\\}
		esc=${esc//\"/\\\"}
		printf '"%s"' "${esc}"
	done
	printf ']}\n'
else
	echo "margen-godot godot-cpp API audit (target Godot ${TARGET_VERSION})"
	echo "-------------------------------------------------------------"
	for line in "${lines[@]}"; do
		echo "${line}"
	done
	echo "-------------------------------------------------------------"
	echo "PASS=${pass} WARN=${warn} FAIL=${fail}"
fi

if [[ "${STRICT}" -eq 1 && "${fail}" -gt 0 ]]; then
	exit 1
fi
exit 0
