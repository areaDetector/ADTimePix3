#!/usr/bin/env bash
# Save a versioned Serval OpenAPI snapshot under documentation/medipix3/drafts/.
# Serval must be running. See integration.md § Serval API reference (OpenAPI).
#
# Usage: fetch-serval-openapi.sh [PORT]
# Example: ./fetch-serval-openapi.sh 8081
set -euo pipefail

PORT="${1:-8081}"
BASE="http://localhost:${PORT}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DRAFTS_DIR="${SCRIPT_DIR}/../drafts"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

mkdir -p "${DRAFTS_DIR}"

curl -sf "${BASE}/openapi.json" -o "${TMP_DIR}/openapi.json"
curl -sf "${BASE}/dashboard" -o "${TMP_DIR}/dashboard.json" 2>/dev/null || echo '{}' > "${TMP_DIR}/dashboard.json"

slug="$(
  python3 - "${TMP_DIR}/openapi.json" "${TMP_DIR}/dashboard.json" <<'PY'
import json, re, sys
spec = json.load(open(sys.argv[1]))
try:
    dash = json.load(open(sys.argv[2]))
except (json.JSONDecodeError, OSError):
    dash = {}
ver = spec.get("info", {}).get("version", "unknown")
build = dash.get("Server", {}).get("SoftwareBuild", "unknown")
ver_slug = re.sub(r"[^A-Za-z0-9._-]+", "-", ver)
print(f"serval-openapi-{ver_slug}-build{build}")
PY
)"

out_json="${DRAFTS_DIR}/${slug}.json"
out_yaml="${DRAFTS_DIR}/${slug}.yaml"

python3 -m json.tool "${TMP_DIR}/openapi.json" > "${out_json}"

if curl -sf "${BASE}/openapi.yaml" -o "${out_yaml}" 2>/dev/null; then
  echo "Wrote ${out_yaml}"
else
  rm -f "${out_yaml}"
  echo "Note: ${BASE}/openapi.yaml not available; JSON only."
fi

echo "Wrote ${out_json}"
python3 -c "import json; print('info.version=' + json.load(open('${out_json}'))['info']['version'])"
