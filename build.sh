#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

resolve_ndk_root() {
  if [[ -n "${ANDROID_NDK_ROOT:-}" ]]; then
    printf '%s\n' "${ANDROID_NDK_ROOT}"
    return
  fi
  if [[ -n "${ANDROID_NDK_HOME:-}" ]]; then
    printf '%s\n' "${ANDROID_NDK_HOME}"
    return
  fi
  if [[ -n "${NDK_ROOT:-}" ]]; then
    printf '%s\n' "${NDK_ROOT}"
    return
  fi

  local candidates=()
  if [[ -n "${HOME:-}" && -d "${HOME}/Android/Sdk/ndk" ]]; then
    while IFS= read -r line; do
      candidates+=("$line")
    done < <(find "${HOME}/Android/Sdk/ndk" -mindepth 1 -maxdepth 1 -type d | sort -V)
  fi

  if [[ "${#candidates[@]}" -eq 0 ]]; then
    return 1
  fi

  printf '%s\n' "${candidates[-1]}"
}

if command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
  PYTHON_BIN="python"
else
  echo "missing required command: python3" >&2
  exit 1
fi

ANDROID_NDK_ROOT="$(resolve_ndk_root || true)"
if [[ -z "${ANDROID_NDK_ROOT}" ]]; then
  echo "unable to resolve ANDROID_NDK_ROOT" >&2
  exit 1
fi

NDBUILD="${ANDROID_NDK_ROOT}/ndk-build"
if [[ ! -x "${NDBUILD}" ]]; then
  echo "ndk-build not found at ${NDBUILD}" >&2
  exit 1
fi

echo "[build] preparing BNM headers"
"${PYTHON_BIN}" "${ROOT_DIR}/scripts/prepare_bnm_headers.py"

rm -rf "${ROOT_DIR}/libs" "${ROOT_DIR}/obj"

echo "[build] building payload with ndk-build"
"${NDBUILD}" \
  -C "${ROOT_DIR}" \
  "NDK_PROJECT_PATH=${ROOT_DIR}" \
  "NDK_APPLICATION_MK=${ROOT_DIR}/jni/Application.mk" \
  "-j$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

: > "${ROOT_DIR}/libs/.gitkeep"
mkdir -p "${ROOT_DIR}/module/system/lib64"
cp "${ROOT_DIR}/libs/arm64-v8a/libbabix_payload.so" "${ROOT_DIR}/module/system/lib64/libbabix_payload.so"
mkdir -p "${ROOT_DIR}/module/zygisk"
cp "${ROOT_DIR}/libs/arm64-v8a/libbabix_zygisk.so" "${ROOT_DIR}/module/zygisk/arm64-v8a.so"

echo "[build] done: ${ROOT_DIR}/libs/arm64-v8a/libbabix_payload.so"
