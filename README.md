# babixGO-native-hooks

Native `libbabix_payload.so` for IL2CPP hooks in Monopoly GO using BNM + Dobby. The project focuses on a stable preload/bootstrap path and direct game-method hooks resolved from `dump.cs`.

## Scope

- Native payload for `arm64-v8a`
- BNM-based IL2CPP discovery
- Dobby-based inline hook installation
- Roll hooks on `Tophat.Client.ClientActions` and `Tophat.Client.DiceRoll`
- Jail hooks on `Tophat.Client.Jail.EscapeJailService`
- CoinFlip hooks on `Tophat.Client.ForeverGames.CoinFlip.CoinFlipService`
- Pickup hooks on `Tophat.Client.BoardPickups` and `Tophat.Common.Rolling.PickupHandler`
- Chance hook on `Tophat.Common.Rolling.CardHandler.DrawCard`
- Speed hooks on `UnityEngine.Time.get/set_timeScale`
- Zygisk-based loader module for process-targeted payload injection

Not included:

- Memory hooks into `libmys_payload.so`

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
│   ├── ipc_feed.cpp
│   ├── ipc_feed.h
│   ├── pattern_scanner.cpp
│   ├── pattern_scanner.h
│   ├── zygisk.hpp
│   ├── zygisk_loader.cpp
│   ├── hooks/
│   │   ├── hook_utils.cpp
│   │   ├── roll_hook.cpp
│   │   ├── jail_hook.cpp
│   │   ├── coinflip_hook.cpp
│   │   ├── pickups_hook.cpp
│   │   ├── chance_hook.cpp
│   │   └── speed_hook.cpp
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
│   ├── zygisk/
│   └── system/
│       └── lib64/
├── scripts/
│   └── prepare_bnm_headers.py
└── test/
    ├── install.sh
    └── test_injection.sh
```

## Why The Build Looks Like This

`BNM` expects a project-specific `GlobalSettings.hpp`. To keep the upstream submodules clean:

1. `scripts/prepare_bnm_headers.py` copies `jni/external/BNM/include` into `jni/generated/BNM/include`
2. the copied `GlobalSettings.hpp` is replaced with the project-local Dobby-enabled version
3. `ndk-build` builds `Dobby` directly from source with the C++ closure-trampoline path enabled
4. the final shared object links `BNM` and `Dobby` in one pass

This avoids leaving either submodule dirty after checkout and avoids the ARM64 assembler issues that Dobby can hit on newer NDKs.

## Requirements

- Android NDK r25+ with `ndk-build`
- Python 3
- `adb`
- Rooted target device with Magisk + Zygisk enabled

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
- `libs/arm64-v8a/libbabix_zygisk.so`
- `module/system/lib64/libbabix_payload.so`
- `module/zygisk/arm64-v8a.so`

## Runtime Model

The payload does not try to hook from the constructor directly.

1. `constructor` starts a detached bootstrap thread
2. the bootstrap thread waits for `libil2cpp.so`
3. `BNM::Loading::TryLoadByDlfcnHandle()` arms BNM on the IL2CPP init path
4. `BNM::Loading::AddOnLoadedEvent()` fires when BNM is fully ready
5. a second worker thread attaches to IL2CPP, waits 3 seconds for game warmup, then installs hooks

That keeps the constructor minimal and avoids blocking the process during library load.

## Installed Hooks

- `Tophat.Client.ClientActions.TophatClientActionsFlusher.Update()`
- `Tophat.Client.DiceRoll.RollService.AttemptRoll()`
- `Tophat.Client.DiceRoll.RollService.SetMultiplier(int)`
- `Tophat.Client.DiceRoll.RollService.GetCurrentMultiplier(bool)`
- `Tophat.Client.Jail.EscapeJailService.BeginRoll()`
- `Tophat.Client.Jail.EscapeJailService.SetState(EscapeJailState)`
- `Tophat.Client.Jail.EscapeJailService.OnRollButtonPressed()`
- `Tophat.Client.ForeverGames.CoinFlip.CoinFlipService.TryFlip()`
- `Tophat.Client.ForeverGames.CoinFlip.CoinFlipService.ChangeAnimationState(CoinFlipAnimationState)`
- `Tophat.Client.ForeverGames.CoinFlip.CoinFlipService.GetMaxFlips()`
- `Tophat.Client.BoardPickups.BoardPickupsService.EvaluateBoardPickups(bool,bool)`
- `Tophat.Client.BoardPickups.BoardPickupsService.CheckForPickupsAwaitingSpawn(int)`
- `Tophat.Client.BoardPickups.BoardPickupsService.SetNextMoveTokenAnimatorParams(int,IEnumerable<...>)`
- `Tophat.Client.BoardPickups.PickupSources.SpecialCurrencyEventPickupSource.OnLandHandler(IClientPickup)`
- `Tophat.Common.Rolling.PickupHandler.ProcessPickups(...)`
- `Tophat.Common.Rolling.CardHandler.DrawCard(MovementContext, IClientActionExecutionContext)`
- `UnityEngine.Time.set_timeScale(float)`
- `UnityEngine.Time.get_timeScale()`

Current hook behavior is intentionally read-only/log-centric (calls original function and logs signal/state).
Run the module smoke test:

```bash
./test/install.sh
```

That script:

- stages the module into `/data/adb/modules_update/babix_native_hooks`
- reboots the device so Magisk activates the module
- launches Monopoly GO
- tails `logcat`

## Magisk Module

For persistent Zygisk loading:

1. copy the built `libbabix_payload.so` into `module/system/lib64/`
2. copy the built `libbabix_zygisk.so` to `module/zygisk/arm64-v8a.so`
3. install the module and reboot

`post-fs-data.sh` now only clears stale `wrap.` properties from older module versions.

## Known Limits

- The MWE assumes early preload. It is not designed for arbitrary late injection.
- `UNITY_VER` is pinned to `222` in the generated BNM config. If Monopoly GO changes Unity major/minor, update `jni/config/BNM/UserSettings/GlobalSettings.hpp`.
- IPC feed is best-effort and defaults to the abstract UNIX datagram socket `@babix_native_hooks` (override via `BABIX_IPC_SOCKET`).
- Pattern scanning currently provides utility-level probes and fallback diagnostics, not full signature databases.
- Late ptrace-style injection now has a fallback path that starts hook installation if BNM callback timing is missed.
