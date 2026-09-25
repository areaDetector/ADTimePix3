#!/usr/bin/env bash
# Capture all MPX3 Serval detector layouts.
# SPDX-License-Identifier: MIT

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)

if [[ ${1:-} == "-h" || ${1:-} == "--help" ]]; then
    exec "${script_dir}/capture_serval_layouts.sh" --help
fi

pv_prefix=${1:-${PV_PREFIX:-MPX3-TEST:cam1:}}
serval_url=${2:-${SERVAL_URL:-http://localhost:8081}}
output_dir=${3:-${OUTPUT_DIR:-${repo_root}/.codex/layout-captures/mpx3}}

exec "${script_dir}/capture_serval_layouts.sh" \
    MPX3 "${pv_prefix}" "${serval_url}" "${output_dir}"
