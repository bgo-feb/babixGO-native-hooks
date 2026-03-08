#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

ADB="${ADB:-adb}"
DEVICE="${DEVICE:-${ADB_SERIAL:-}}"
PACKAGE="${PACKAGE:-com.scopely.monopolygo}"
MODULE_ID="${MODULE_ID:-babix_native_hooks}"
MODULE_ROOT="${ROOT_DIR}/module"
PAYLOAD="${MODULE_ROOT}/system/lib64/libbabix_payload.so"
ZYGISK_LOADER="${MODULE_ROOT}/zygisk/arm64-v8a.so"

if [[ ! -f "${PAYLOAD}" || ! -f "${ZYGISK_LOADER}" ]]; then
  echo "module artifacts missing. expected:" >&2
  echo "  ${PAYLOAD}" >&2
  echo "  ${ZYGISK_LOADER}" >&2
  exit 1
fi

ADB_ARGS=("${ADB}")
if [[ -n "${DEVICE}" ]]; then
  ADB_ARGS+=("-s" "${DEVICE}")
fi

adb_cmd() {
  "${ADB_ARGS[@]}" "$@"
}

root_shell() {
  adb_cmd shell "su -c '$1'"
}

wait_boot_completed() {
  for _ in $(seq 1 120); do
    if [[ "$(adb_cmd shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]]; then
      return 0
    fi
    sleep 2
  done
  echo "timed out waiting for sys.boot_completed=1" >&2
  exit 1
}

echo "[test] stopping ${PACKAGE}"
adb_cmd shell am force-stop "${PACKAGE}" >/dev/null 2>&1 || true

echo "[test] pushing module tree"
adb_cmd push "${MODULE_ROOT}" /data/local/tmp/babix_module >/dev/null

echo "[test] staging module in /data/adb/modules_update/${MODULE_ID}"
root_shell "rm -rf /data/adb/modules_update/${MODULE_ID} /data/adb/modules/${MODULE_ID}"
root_shell "mkdir -p /data/adb/modules_update/${MODULE_ID}"
root_shell "cp -af /data/local/tmp/babix_module/. /data/adb/modules_update/${MODULE_ID}/"
root_shell "chmod 755 /data/adb/modules_update/${MODULE_ID}/post-fs-data.sh || true"
root_shell "chmod 644 /data/adb/modules_update/${MODULE_ID}/module.prop /data/adb/modules_update/${MODULE_ID}/system/lib64/libbabix_payload.so /data/adb/modules_update/${MODULE_ID}/zygisk/arm64-v8a.so || true"
root_shell "chown -R 0:0 /data/adb/modules_update/${MODULE_ID}"
root_shell "chcon u:object_r:system_file:s0 /data/adb/modules_update/${MODULE_ID}/system/lib64/libbabix_payload.so || true"
root_shell "chcon u:object_r:system_file:s0 /data/adb/modules_update/${MODULE_ID}/zygisk/arm64-v8a.so || true"

echo "[test] rebooting to activate module"
adb_cmd reboot
adb_cmd wait-for-device
wait_boot_completed

echo "[test] launching ${PACKAGE}"
adb_cmd logcat -c
adb_cmd shell monkey -p "${PACKAGE}" -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1

echo "[test] tailing logcat"
exec "${ADB_ARGS[@]}" logcat -v time -s BabixZygisk:D BabixPayload:D HookManager:D HookResolver:D RollHooks:D JailHooks:D CoinFlipHooks:D PickupHooks:D ChanceHooks:D SpeedHooks:D BabixBNM:D *:E
