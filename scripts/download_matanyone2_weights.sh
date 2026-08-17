#!/usr/bin/env bash
# Download official MatAnyone2 weights into models/matanyone2/
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${ROOT}/models/matanyone2"
URL="https://github.com/pq-yang/MatAnyone2/releases/download/v1.0.0/matanyone2.pth"
mkdir -p "${OUT}"
echo "Downloading ${URL}"
curl -fL --retry 3 --retry-delay 2 "${URL}" -o "${OUT}/matanyone2.pth.partial"
mv "${OUT}/matanyone2.pth.partial" "${OUT}/matanyone2.pth"
cat > "${OUT}/SOURCE.txt" <<EOF
MatAnyone2 checkpoint
Source: ${URL}
Upstream: https://github.com/pq-yang/MatAnyone2
License: see upstream (NTU S-Lab / project terms)
EOF
ls -lh "${OUT}/matanyone2.pth"
