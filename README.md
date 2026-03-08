# babixGO-native-hooks

Minimal Working Example for a native `libbabix_payload.so` that hooks IL2CPP methods in Monopoly GO without feature logic. The project is intentionally limited to a stable loader path, one diagnostic hook, and deployment helpers.

## Scope

- Native payload for `arm64-v8a`
- BNM-based IL2CPP discovery
- Dobby-based inline hook installation
- One low-risk smoke-test hook on `UnityEngine.Debug.Log(object)`
- Magisk-style persistent preload plus a faster direct test script

Not included:

- Roll/jail/speed feature hooks
- IPC / socket feed
- Pattern scanning
- Late ptrace injection support

## Repository Layout

```text
babixGO-native-hooks/
├── build.ps1
├── build.sh
├── jni/
│   ├── Android.mk
│   ├── Application.mk
│   ├── main.cpp
│   ├── hook_manager.cpp
│   ├── hook_manager.h
│   ├── hooks/
│   │   └── roll_hook.cpp
│   ├── config/
│   │   └── BNM/
│   │       └── UserSettings/
│   │           └── GlobalSettings.hpp
│   ├── external/
│   │   ├── BNM/
│   │   └── Dobby/
│   └── generated/
├── libs/
├── module/
│   ├── module.prop
│   ├── post-fs-data.sh
│   └── system/
│       └── lib64/
├── scripts/
│   └── prepare_bnm_headers.py
└── test/
    ├── install.sh
    └── test_injection.sh
```

## Why The Build Looks Like This

`BNM` expects a project-specific `GlobalSettings.hpp` and does not ship a ready-to-use `ndk-build` integration for `Dobby`. To keep the upstream submodules clean:

1. `scripts/prepare_bnm_headers.py` copies `jni/external/BNM/include` into `jni/generated/BNM/include`
2. the copied `GlobalSettings.hpp` is replaced with the project-local Dobby-enabled version
3. `build.ps1` / `build.sh` build `Dobby` once with CMake and copy `libdobby.a` into `jni/external/Dobby/prebuilt/arm64-v8a/`
4. `ndk-build` links the final shared object against that prebuilt archive

This avoids leaving either submodule dirty after checkout.

## Requirements

- Android NDK r25+ with `ndk-build`
- CMake
- Ninja
- Python 3
- `adb`
- Rooted target device if you want to use the direct wrap-based smoke test

## Build

Windows PowerShell:

```powershell
.\build.ps1
```

POSIX shell:

```bash
./build.sh
```

Successful builds produce:

- `libs/arm64-v8a/libbabix_payload.so`
- `module/system/lib64/libbabix_payload.so`

## Runtime Model

The payload does not try to hook from the constructor directly.

1. `constructor` starts a detached bootstrap thread
2. the bootstrap thread waits for `libil2cpp.so`
3. `BNM::Loading::TryLoadByDlfcnHandle()` arms BNM on the IL2CPP init path
4. `BNM::Loading::AddOnLoadedEvent()` fires when BNM is fully ready
5. a second worker thread attaches to IL2CPP, waits 3 seconds for game warmup, then installs hooks

That keeps the constructor minimal and avoids blocking the process during library load.

## Smoke Test

The default diagnostic hook is:

- `UnityEngine.Debug.Log(object)` from `UnityEngine.CoreModule.dll`

Hook behavior:

- calls the original first
- logs string messages with `[Unity]` prefix
- avoids heavy work for non-string payloads

Run the direct smoke test:

```bash
./test/install.sh
```

That script:

- pushes the built payload to `/data/local/tmp/libbabix_payload.so`
- sets `wrap.com.scopely.monopolygo=LD_PRELOAD=/data/local/tmp/libbabix_payload.so`
- restarts Monopoly GO
- tails `logcat`

## Magisk Module

For persistent preload:

1. copy the built `libbabix_payload.so` into `module/system/lib64/`
2. install the `module/` directory as a Magisk module
3. reboot or rerun `post-fs-data.sh`

`post-fs-data.sh` sets the `wrap.` property at boot. It does not wait for the game PID because that would be too late for `LD_PRELOAD`.

## Known Limits

- The MWE assumes early preload. It is not designed for arbitrary late injection.
- `UNITY_VER` is pinned to `222` in the generated BNM config. If Monopoly GO changes Unity major/minor, update `jni/config/BNM/UserSettings/GlobalSettings.hpp`.
- The diagnostic hook proves loader stability, not gameplay offsets.

