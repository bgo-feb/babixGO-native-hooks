#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing required command: $1" >&2
    exit 1
  fi
}

resolve_cmake_binary() {
  if command -v cmake >/dev/null 2>&1; then
    command -v cmake
    return
  fi

  if [[ -n "${LOCALAPPDATA:-}" ]]; then
    local sdk_cmake_root="${LOCALAPPDATA}/Android/Sdk/cmake"
    if [[ -d "${sdk_cmake_root}" ]]; then
      find "${sdk_cmake_root}" -path '*/bin/cmake.exe' | sort -V | tail -n 1
      return
    fi
  fi

  return 1
}

resolve_ninja_binary() {
  if command -v ninja >/dev/null 2>&1; then
    command -v ninja
    return
  fi

  if [[ -n "${LOCALAPPDATA:-}" ]]; then
    local sdk_cmake_root="${LOCALAPPDATA}/Android/Sdk/cmake"
    if [[ -d "${sdk_cmake_root}" ]]; then
      find "${sdk_cmake_root}" -path '*/bin/ninja.exe' | sort -V | tail -n 1
      return
    fi
  fi

  return 1
}

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

CMAKE_BIN="$(resolve_cmake_binary || true)"
NINJA_BIN="$(resolve_ninja_binary || true)"
if [[ -z "${CMAKE_BIN}" ]]; then
  echo "unable to resolve cmake" >&2
  exit 1
fi
if [[ -z "${NINJA_BIN}" ]]; then
  echo "unable to resolve ninja" >&2
  exit 1
fi
export PATH="$(dirname "${NINJA_BIN}"):${PATH}"

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

DOBBY_BUILD_DIR="${ROOT_DIR}/out/dobby/android-arm64"
DOBBY_PREBUILT_DIR="${ROOT_DIR}/jni/external/Dobby/prebuilt/arm64-v8a"

echo "[build] configuring Dobby"
"${CMAKE_BIN}" \
  -S "${ROOT_DIR}/jni/external/Dobby" \
  -B "${DOBBY_BUILD_DIR}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=26 \
  -DANDROID_STL=c++_static \
  -DDOBBY_DEBUG=OFF \
  -DPlugin.SymbolResolver=OFF \
  -DPlugin.ImportTableReplace=OFF \
  -DPlugin.Android.BionicLinkerUtil=OFF \
  -DDOBBY_BUILD_EXAMPLE=OFF \
  -DDOBBY_BUILD_TEST=OFF

echo "[build] building Dobby static library"
"${CMAKE_BIN}" --build "${DOBBY_BUILD_DIR}" --target dobby_static --parallel

mkdir -p "${DOBBY_PREBUILT_DIR}"
cp "${DOBBY_BUILD_DIR}/libdobby.a" "${DOBBY_PREBUILT_DIR}/libdobby.a"

rm -rf "${ROOT_DIR}/libs" "${ROOT_DIR}/obj"

echo "[build] building payload with ndk-build"
"${NDBUILD}" \
  -C "${ROOT_DIR}" \
  "NDK_PROJECT_PATH=${ROOT_DIR}" \
  "NDK_APPLICATION_MK=${ROOT_DIR}/jni/Application.mk" \
  "-j$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

mkdir -p "${ROOT_DIR}/module/system/lib64"
cp "${ROOT_DIR}/libs/arm64-v8a/libbabix_payload.so" "${ROOT_DIR}/module/system/lib64/libbabix_payload.so"

echo "[build] done: ${ROOT_DIR}/libs/arm64-v8a/libbabix_payload.so"
