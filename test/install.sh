#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

ADB="${ADB:-adb}"
PACKAGE="${PACKAGE:-com.scopely.monopolygo}"
PAYLOAD="${PAYLOAD:-${ROOT_DIR}/libs/arm64-v8a/libbabix_payload.so}"
REMOTE_PAYLOAD="${REMOTE_PAYLOAD:-/data/local/tmp/libbabix_payload.so}"

if [[ ! -f "${PAYLOAD}" ]]; then
  echo "payload not found: ${PAYLOAD}" >&2
  exit 1
fi

root_shell() {
  "${ADB}" shell su -c "$1"
}

echo "[test] stopping ${PACKAGE}"
"${ADB}" shell am force-stop "${PACKAGE}" >/dev/null 2>&1 || true

echo "[test] pushing payload"
"${ADB}" push "${PAYLOAD}" "${REMOTE_PAYLOAD}" >/dev/null
root_shell "chmod 644 ${REMOTE_PAYLOAD}"

echo "[test] configuring wrap property"
root_shell "if command -v resetprop >/dev/null 2>&1; then resetprop -n wrap.${PACKAGE} LD_PRELOAD=${REMOTE_PAYLOAD}; else setprop wrap.${PACKAGE} LD_PRELOAD=${REMOTE_PAYLOAD}; fi"

"${ADB}" logcat -c

echo "[test] launching ${PACKAGE}"
"${ADB}" shell monkey -p "${PACKAGE}" -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1

echo "[test] tailing logcat"
exec "${ADB}" logcat -v time -s BabixPayload:D HookManager:D BabixBNM:D *:E

