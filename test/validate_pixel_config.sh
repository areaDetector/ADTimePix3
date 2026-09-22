#!/usr/bin/env bash
# Live IOC validation for Serval PixelConfig response handling.
# SPDX-License-Identifier: MIT

set -uo pipefail

pv_prefix=${1:-${PV_PREFIX:-TPX3-TEST:cam1:}}

usage() {
    printf 'Usage: %s [PV_PREFIX]\n' "${0##*/}"
    printf 'Example: %s MPX3-TEST:cam1:\n' "${0##*/}"
}

if [[ ${pv_prefix} == "-h" || ${pv_prefix} == "--help" ]]; then
    usage
    exit 0
fi

for command_name in caput caget; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf 'ERROR: required command not found: %s\n' "${command_name}" >&2
        exit 2
    fi
done

read_scalar() {
    caget -t -w 5 "$1"
}

read_string() {
    caget -S -t -w 5 "$1"
}

if ! detector_type=$(read_scalar "${pv_prefix}DetType_RBV"); then
    printf 'ERROR: cannot read %sDetType_RBV\n' "${pv_prefix}" >&2
    exit 2
fi
if ! chip_count=$(read_scalar "${pv_prefix}NChips_RBV"); then
    printf 'ERROR: cannot read %sNChips_RBV\n' "${pv_prefix}" >&2
    exit 2
fi
if [[ ! ${chip_count} =~ ^[1-9][0-9]*$ ]] || (( chip_count > 64 )); then
    printf 'ERROR: invalid chip count: %s\n' "${chip_count}" >&2
    exit 2
fi

case ${detector_type,,} in
    tpx3|timepix3)
        expected_length=65536
        ;;
    mpx3|medipix3)
        expected_length=131072
        ;;
    *)
        printf 'ERROR: unsupported detector type: %s\n' "${detector_type}" >&2
        exit 2
        ;;
esac

printf 'PixelConfig validation: prefix=%s detector=%s chips=%s expected-bytes/chip=%s\n' \
    "${pv_prefix}" "${detector_type}" "${chip_count}" "${expected_length}"

if ! caput -c -w 30 "${pv_prefix}RefreshPixelConfig" 1 >/dev/null; then
    printf 'ERROR: RefreshPixelConfig failed or timed out\n' >&2
    exit 2
fi

printf '%-6s %10s %7s %12s  %s\n' CHIP LENGTH MATCH MISMATCH STATUS
failures=0
for ((chip = 0; chip < chip_count; ++chip)); do
    chip_prefix="${pv_prefix}CHIP${chip}_PixelConfig"
    if ! length=$(read_scalar "${chip_prefix}Len_RBV") ||
       ! match=$(read_scalar "${chip_prefix}MatchBPC_RBV") ||
       ! mismatch=$(read_scalar "${chip_prefix}MismatchBytes_RBV") ||
       ! status=$(read_string "${chip_prefix}Status_RBV"); then
        printf 'CHIP%-2d ERROR: failed to read one or more result PVs\n' "${chip}" >&2
        ((failures += 1))
        continue
    fi

    printf 'CHIP%-2d %10s %7s %12s  %s\n' \
        "${chip}" "${length}" "${match}" "${mismatch}" "${status}"

    if [[ ${length} != "${expected_length}" || ${match} != "1" ||
          ${mismatch} != "0" || ${status} != "OK, matches BPC" ]]; then
        ((failures += 1))
    fi
done

if (( failures != 0 )); then
    printf 'FAIL: %d chip(s) did not match the expected PixelConfig/BPC result\n' \
        "${failures}" >&2
    exit 1
fi

printf 'PASS: all %s chip PixelConfig responses are valid and match the BPC\n' \
    "${chip_count}"
