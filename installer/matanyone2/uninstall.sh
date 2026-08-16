#!/usr/bin/env bash
set -euo pipefail
OFX_DIR="${OFX_PLUGIN_DIR:-/usr/OFX/Plugins}"
PREFIX="${EMBR_MATANYONE2_HOME:-/opt/Embr/EmbrMatAnyone2}"
run_root() {
  if [[ "$(id -u)" -eq 0 ]]; then "$@"; elif command -v sudo >/dev/null 2>&1; then sudo "$@"; else echo "root required"; exit 1; fi
}
echo "Remove ${OFX_DIR}/EmbrMatAnyone2.ofx.bundle and ${PREFIX} ?"
read -r -p "[y/N] " a
case "$a" in y|Y|yes|YES) ;; *) exit 0 ;; esac
run_root rm -rf "${OFX_DIR}/EmbrMatAnyone2.ofx.bundle" "${PREFIX}"
echo "done"
