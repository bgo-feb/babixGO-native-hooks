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
- Magisk-style persistent preload plus a faster direct test script

Not included:

- Memory hooks into `libmys_payload.so`
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

Current hook behavior is intentionally read-only/log-centric (calls original function and logs signal/state). It does not patch `libmys_payload.so` globals.

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
- Hooks currently log/observe method traffic and parameters. Feature state extraction and IPC publishing are still a separate step.
