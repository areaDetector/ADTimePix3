#!/usr/bin/env bash
# Capture Serval Layout metadata for all detector orientations.
# SPDX-License-Identifier: MIT

set -euo pipefail

orientations=(
    UP
    RIGHT
    DOWN
    LEFT
    UP_MIRRORED
    RIGHT_MIRRORED
    DOWN_MIRRORED
    LEFT_MIRRORED
)

usage() {
    cat <<'EOF'
Usage:
  capture_serval_layouts.sh FAMILY PV_PREFIX [SERVAL_URL] [OUTPUT_DIR]

FAMILY must be MPX3 or TPX3. The script writes one JSON file per detector
orientation, containing only the Layout object returned by GET /detector.

Examples:
  capture_serval_layouts.sh MPX3 MPX3-TEST:cam1:
  capture_serval_layouts.sh TPX3 TPX3-TEST:cam1: http://localhost:8081 ./layouts

The original detector orientation is restored on exit. Existing output files
are atomically replaced. EPICS Channel Access environment variables are honored.
EOF
}

if [[ ${1:-} == "-h" || ${1:-} == "--help" ]]; then
    usage
    exit 0
fi

if (( $# < 2 || $# > 4 )); then
    usage >&2
    exit 2
fi

family=${1^^}
pv_prefix=$2
serval_url=${3:-http://localhost:8081}
output_dir=${4:-.}

case ${family} in
    MPX3)
        file_prefix=mpx3
        expected_detector=mpx3
        ;;
    TPX3)
        file_prefix=tpx3
        expected_detector=tpx3
        ;;
    *)
        printf 'ERROR: FAMILY must be MPX3 or TPX3, got: %s\n' "$1" >&2
        exit 2
        ;;
esac

for command_name in caput caget wget jq mktemp mkdir mv; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf 'ERROR: required command not found: %s\n' "${command_name}" >&2
        exit 2
    fi
done

set_pv="${pv_prefix}DetOrient"
readback_pv="${pv_prefix}DetOrient_RBV"
detector_type_pv="${pv_prefix}DetType_RBV"
endpoint="${serval_url%/}/detector"

detector_type=$(caget -t -w 5 "${detector_type_pv}")
if [[ ${detector_type,,} != "${expected_detector}" ]]; then
    printf 'ERROR: %s reports detector type %s; expected %s\n' \
        "${detector_type_pv}" "${detector_type}" "${family}" >&2
    exit 2
fi

original_orientation=$(caget -t -n -w 5 "${readback_pv}")
if [[ ! ${original_orientation} =~ ^[0-7]$ ]]; then
    printf 'ERROR: %s returned invalid orientation index: %s\n' \
        "${readback_pv}" "${original_orientation}" >&2
    exit 2
fi

mkdir -p "${output_dir}"
temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/capture-serval-layouts.XXXXXX")

restore_orientation() {
    local exit_status=$?
    trap - EXIT INT TERM
    if [[ -n ${original_orientation:-} ]]; then
        printf 'Restoring %s to %s (%s)\n' \
            "${set_pv}" "${original_orientation}" \
            "${orientations[original_orientation]}"
        caput -c -n -w 30 "${set_pv}" "${original_orientation}" >/dev/null || \
            printf 'WARNING: failed to restore the original detector orientation\n' >&2
    fi
    if [[ -n ${temp_dir:-} && -d ${temp_dir} ]]; then
        rm -r -- "${temp_dir}"
    fi
    exit "${exit_status}"
}
trap restore_orientation EXIT INT TERM

wait_for_orientation_readback() {
    local expected_index=$1
    local attempt actual
    for ((attempt = 1; attempt <= 40; ++attempt)); do
        actual=$(caget -t -n -w 5 "${readback_pv}") || actual=-1
        if [[ ${actual} == "${expected_index}" ]]; then
            return 0
        fi
        sleep 0.25
    done
    printf 'ERROR: %s did not reach orientation %s (%s)\n' \
        "${readback_pv}" "${expected_index}" "${orientations[expected_index]}" >&2
    return 1
}

capture_layout() {
    local expected_name=$1
    local destination=$2
    local raw_file="${temp_dir}/detector.json"
    local layout_file="${temp_dir}/layout.json"
    local attempt

    for ((attempt = 1; attempt <= 20; ++attempt)); do
        if wget --quiet --timeout=10 --tries=1 -O "${raw_file}" "${endpoint}" &&
           jq -e --arg orientation "${expected_name}" --arg family "${family}" '
               (.Layout | type) == "object" and
               .Layout.DetectorOrientation == $orientation and
               .Layout.Original.ChipType == $family and
               .Layout.Rotated.ChipType == $family and
               ((.Layout.Original.Chips | type) == "array") and
               ((.Layout.Rotated.Chips | type) == "array") and
               (.Layout.Original.Chips | length) > 0 and
               (.Layout.Rotated.Chips | length) > 0
           ' "${raw_file}" >/dev/null; then
            jq '{Layout: .Layout}' "${raw_file}" >"${layout_file}"
            mv -f -- "${layout_file}" "${destination}"
            return 0
        fi
        sleep 0.25
    done

    printf 'ERROR: Serval did not return a valid %s %s layout from %s\n' \
        "${family}" "${expected_name}" "${endpoint}" >&2
    return 1
}

printf 'Capturing %s layouts from %s using PV prefix %s\n' \
    "${family}" "${endpoint}" "${pv_prefix}"
printf 'Original orientation: %s (%s)\n' \
    "${original_orientation}" "${orientations[original_orientation]}"

for index in "${!orientations[@]}"; do
    orientation=${orientations[index]}
    destination="${output_dir}/${file_prefix}-${orientation}.json"

    printf '[%d/8] Setting %s and capturing %s\n' \
        "$((index + 1))" "${orientation}" "${destination}"
    caput -c -n -w 30 "${set_pv}" "${index}" >/dev/null
    wait_for_orientation_readback "${index}"
    capture_layout "${orientation}" "${destination}"
done

printf 'Captured all eight %s layouts in %s\n' "${family}" "${output_dir}"
