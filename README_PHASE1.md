# Phase 1: Python Memory Reader MVP

## Overview
- This MVP adds a small Python memory reader for Monopoly GO.
- It reads process memory through `/proc/<pid>/mem`, so root access is required on the Android device or emulator.
- Phase 1 only supports reading the live dice balance.

## What Phase 1 Reads
- The dice balance is read from a manually supplied absolute pointer to the live `WithBuddies.Common.UserInventoryLineItemDto` for `Tophat.Common.Economy.TophatCommodityKeys.ROLLS`.
- The current balance is stored in `_count` at offset `0x10`.
- The value is read as `int64`.

## Why `RollModel + 0x18` Is Not Used
- `Tophat.Client.DiceRoll.RollModel.<Dice> @ 0x18` is not the live dice balance.
- In the current dump it is a `System.Int32[]`, not a scalar roll counter.
- Phase 1 therefore uses the inventory line-item path instead of a `RollModel` field.

## Requirements
- Monopoly GO must be running.
- The target package is `com.scopely.monopolygo`.
- The target process must expose `libil2cpp.so` in `/proc/<pid>/maps`.
- Root access is required to open `/proc/<pid>/mem`.
- Python 3 must be available on the device or emulator.

## Files
- `core/memory_reader.py`: core reader implementation
- `tests/test_memory_reader.py`: unit tests
- `examples/test_memory_mvp.py`: manual test script for a real device

## Manual Anchor Requirement
- Phase 1 does not auto-discover the live `UserInventoryLineItemDto` pointer.
- You must obtain the absolute address of the `ROLLS` line-item with your preferred runtime memory analysis workflow.
- That address is session-specific and may change every time the app restarts.
- Automatic dictionary walking and pointer discovery are out of scope for Phase 1.

## Local Test
Run the unit tests from the repo root:

```bash
python -m unittest discover -s tests -v
```

## Device Test
Push the repo structure needed by the script instead of pushing only the example file.

```bash
adb shell "mkdir -p /data/local/tmp/babixgo-mem"
adb push core /data/local/tmp/babixgo-mem/
adb push examples /data/local/tmp/babixgo-mem/
adb shell "cd /data/local/tmp/babixgo-mem && python3 examples/test_memory_mvp.py --dice-line-item-addr 0x7f12345678"
```

## Expected Output
```text
[INFO] === Memory Reader MVP Test ===
[INFO] Initializing memory reader...
[INFO] Initialization succeeded.
[INFO] PID: 12345
[INFO] Base address: 0x7123456000
[INFO] Dice count: 42
```

## Known Limitations
- Only dice are implemented.
- The dice anchor pointer must be supplied manually.
- No automatic offset or pointer discovery is included.
- No multi-version support is included.
- The reader is read-only and does not modify game memory.
