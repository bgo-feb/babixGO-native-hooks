"""Minimal IL2CPP memory reader for Monopoly GO Phase 1."""

import logging
import os
import struct
import subprocess
from typing import Any, Optional


logger = logging.getLogger(__name__)


class IL2CPPMemoryReader:
    """Read selected Monopoly GO values from `/proc/<pid>/mem`.

    Phase 1 intentionally keeps discovery simple:
    - PID is resolved from the package name.
    - `libil2cpp.so` base is resolved from `/proc/<pid>/maps`.
    - Dice are read from a manually supplied absolute heap pointer to the live
      `WithBuddies.Common.UserInventoryLineItemDto` instance for `ROLLS`.
    """

    DICE_LINE_ITEM_COUNT_OFFSET = 0x10

    def __init__(
        self,
        package_name: str = "com.scopely.monopolygo",
        lib_name: str = "libil2cpp.so",
        dice_line_item_address: Optional[int] = None,
    ) -> None:
        """Initialize the reader state.

        Args:
            package_name: Android package to target.
            lib_name: Shared library used to resolve the IL2CPP base.
            dice_line_item_address: Absolute address of the live
                `UserInventoryLineItemDto` for `ROLLS`.
        """
        self.package_name = package_name
        self.lib_name = lib_name
        self.dice_line_item_address = dice_line_item_address
        self.pid: Optional[int] = None
        self.base_address: Optional[int] = None
        self.mem_file: Optional[int] = None

    def initialize(self) -> bool:
        """Initialize PID, base address, and `/proc/<pid>/mem`.

        Returns:
            True if initialization completed successfully, otherwise False.
        """
        self.close()
        self.pid = None
        self.base_address = None

        self.pid = self._find_process_id()
        if self.pid is None:
            logger.error("Failed to resolve PID for package '%s'.", self.package_name)
            return False

        self.base_address = self._get_library_base_address()
        if self.base_address is None:
            logger.error(
                "Failed to resolve base address for '%s' in PID %s.",
                self.lib_name,
                self.pid,
            )
            return False

        mem_path = f"/proc/{self.pid}/mem"
        try:
            self.mem_file = os.open(mem_path, os.O_RDONLY)
        except OSError as exc:
            logger.error("Failed to open %s: %s", mem_path, exc)
            self.mem_file = None
            return False

        logger.info(
            "Memory reader initialized (pid=%s, base=0x%x).",
            self.pid,
            self.base_address,
        )
        return True

    def _find_process_id(self) -> Optional[int]:
        """Find the target PID via `pidof` and then `ps`.

        Returns:
            PID as an integer, or None if the process cannot be found.
        """
        try:
            result = subprocess.run(
                ["pidof", self.package_name],
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode == 0 and result.stdout.strip():
                pid_token = result.stdout.strip().split()[0]
                try:
                    pid = int(pid_token)
                    logger.debug("Resolved PID %s via pidof.", pid)
                    return pid
                except ValueError:
                    logger.warning("Unexpected pidof output: %r", result.stdout)
        except OSError as exc:
            logger.debug("pidof failed for '%s': %s", self.package_name, exc)

        try:
            result = subprocess.run(
                ["ps"],
                capture_output=True,
                text=True,
                check=False,
            )
        except OSError as exc:
            logger.error("Failed to execute ps while resolving PID: %s", exc)
            return None

        for line in result.stdout.splitlines():
            pid = self._parse_pid_from_ps_line(line)
            if pid is not None:
                logger.debug("Resolved PID %s via ps fallback.", pid)
                return pid

        logger.warning("Process '%s' not found via pidof or ps.", self.package_name)
        return None

    def _parse_pid_from_ps_line(self, line: str) -> Optional[int]:
        """Extract the PID from a single `ps` output line.

        Args:
            line: One line of `ps` output.

        Returns:
            PID if the line matches the target package, otherwise None.
        """
        stripped = line.strip()
        if not stripped:
            return None

        parts = stripped.split()
        if not parts or parts[-1] != self.package_name:
            return None

        for token in parts[:-1]:
            if token.isdigit():
                return int(token)

        return None

    def _get_library_base_address(self) -> Optional[int]:
        """Parse `/proc/<pid>/maps` for the first `libil2cpp.so` mapping.

        Returns:
            Base address as an integer, or None on failure.
        """
        if self.pid is None:
            logger.warning("Cannot resolve library base address without a PID.")
            return None

        maps_path = f"/proc/{self.pid}/maps"
        try:
            with open(maps_path, "r", encoding="utf-8") as maps_file:
                for line in maps_file:
                    if self.lib_name not in line:
                        continue

                    range_token = line.split(maxsplit=1)[0]
                    start_addr = range_token.split("-", maxsplit=1)[0]
                    base_address = int(start_addr, 16)
                    logger.debug(
                        "Resolved %s base address: 0x%x",
                        self.lib_name,
                        base_address,
                    )
                    return base_address
        except (OSError, ValueError) as exc:
            logger.error("Failed to parse %s: %s", maps_path, exc)
            return None

        logger.warning("Library '%s' not found in %s.", self.lib_name, maps_path)
        return None

    def _unpack_data(self, raw_data: bytes, data_type: str) -> Optional[Any]:
        """Convert raw bytes to the requested type.

        Args:
            raw_data: Bytes read from process memory.
            data_type: Requested output type.

        Returns:
            Decoded value or None for unsupported types.
        """
        if data_type == "raw":
            return raw_data
        if data_type == "int32":
            return struct.unpack("<i", raw_data)[0]
        if data_type == "int64":
            return struct.unpack("<q", raw_data)[0]
        if data_type == "float":
            return struct.unpack("<f", raw_data)[0]

        logger.error("Unsupported data_type requested: %s", data_type)
        return None

    def _read_absolute(self, address: int, size: int, data_type: str = "int32") -> Optional[Any]:
        """Read bytes from an absolute process address.

        Args:
            address: Absolute process memory address.
            size: Number of bytes to read.
            data_type: `int32`, `int64`, `float`, or `raw`.

        Returns:
            Decoded value or None on error.
        """
        if self.mem_file is None:
            logger.warning("Cannot read memory before opening /proc/<pid>/mem.")
            return None

        if size <= 0:
            logger.error("Invalid read size requested: %s", size)
            return None

        try:
            os.lseek(self.mem_file, address, os.SEEK_SET)
            raw_data = os.read(self.mem_file, size)
            if len(raw_data) != size:
                logger.warning(
                    "Short read at 0x%x: expected %s bytes, got %s bytes.",
                    address,
                    size,
                    len(raw_data),
                )
                return None
            return self._unpack_data(raw_data, data_type)
        except (OSError, struct.error) as exc:
            logger.error("Memory read failed at 0x%x: %s", address, exc)
            return None

    def read_memory(self, offset: int, size: int, data_type: str = "int32") -> Optional[Any]:
        """Read memory relative to the `libil2cpp.so` base address.

        Args:
            offset: Offset from `self.base_address`.
            size: Number of bytes to read.
            data_type: `int32`, `int64`, `float`, or `raw`.

        Returns:
            Decoded value or None on error.
        """
        if self.base_address is None:
            logger.warning("Cannot read base-relative memory without a base address.")
            return None

        absolute_addr = self.base_address + offset
        return self._read_absolute(absolute_addr, size, data_type)

    def get_dice_count(self) -> Optional[int]:
        """Read the live dice balance from the manual line-item pointer.

        Returns:
            Dice count as `int`, or None when the anchor pointer is missing or
            the read fails.
        """
        if self.dice_line_item_address is None:
            logger.warning(
                "dice_line_item_address is not configured. "
                "Phase 1 requires a manual absolute pointer to the live "
                "UserInventoryLineItemDto for ROLLS."
            )
            return None

        return self._read_absolute(
            self.dice_line_item_address + self.DICE_LINE_ITEM_COUNT_OFFSET,
            8,
            "int64",
        )

    def is_valid(self) -> bool:
        """Check whether the reader is initialized and usable."""
        return (
            self.pid is not None
            and self.base_address is not None
            and self.mem_file is not None
        )

    def close(self) -> None:
        """Close the memory file descriptor if it is open."""
        if self.mem_file is None:
            return

        try:
            os.close(self.mem_file)
        except OSError as exc:
            logger.warning("Failed to close mem file descriptor %s: %s", self.mem_file, exc)
        finally:
            self.mem_file = None


class MysRollServiceDiscovery:
    """Discover the live RollService pointer via libmys_payload.so globals.

    libmys_payload.so hooks ``RollService::ToggleMultiplier`` (IL2CPP offset
    ``0x40493E8``) and captures the ``self`` pointer on first call:

    .. code-block:: cpp

        extern "C" void hooked_ToggleMultiplier(void* self) {
            if (self != nullptr && g_rollServiceInstance == nullptr)
                g_rollServiceInstance = self;   // <-- what we read
            ...
        }

    Pointer chain (offsets from dump.cs):
        ``libmys[g_rollServiceInstance]``  →  ``RollService*``
        ``RollService* + 0x10``            →  ``RollModel*``   (serverModel field)
        ``RollModel*   + 0x18``            →  ``int32``        (DicekBackingField)

    Prerequisites:
        - ``libmys_payload.so`` must be injected and active.
        - The user must have tapped the multiplier button at least once so that
          ``hooked_ToggleMultiplier`` has stored ``g_rollServiceInstance``.
    """

    # Offsets from dump.cs (verified against v1.63.1 Mono dump)
    _ROLLSERVICE_TO_SERVERMODEL: int = 0x10   # RollService::serverModel
    _ROLLMODEL_TO_DICE: int = 0x18            # RollModel::DicekBackingField (int32)

    # Scan boundaries for g_rollServiceInstance inside libmys data segment
    _SCAN_RANGE: int = 0x100000   # 1 MB – covers .data + .bss comfortably
    _SCAN_STEP: int = 8           # pointer-aligned scan

    # Sanity bounds for a plausible dice count
    _DICE_MIN: int = 0
    _DICE_MAX: int = 9999

    MYS_LIB_NAME: str = "libmys_payload.so"

    def __init__(self, pid: int, mem_file: int) -> None:
        """
        Args:
            pid: PID of the Monopoly GO process.
            mem_file: Open file descriptor for ``/proc/<pid>/mem``.
        """
        self._pid = pid
        self._mem_file = mem_file
        self._mys_base: Optional[int] = self._get_mys_base()
        self._g_rollservice_addr: Optional[int] = None

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    @property
    def available(self) -> bool:
        """True when libmys_payload.so is loaded in the target process."""
        return self._mys_base is not None

    def get_dice_count(self) -> Optional[int]:
        """Return the current dice count via the mys pointer chain.

        Returns:
            Dice count, or ``None`` on any error.  Call ``last_error`` to get
            a human-readable explanation when ``None`` is returned.
        """
        if not self.available:
            logger.warning(
                "MysRollServiceDiscovery: %s not found in /proc/%s/maps.",
                self.MYS_LIB_NAME,
                self._pid,
            )
            return None

        if self._g_rollservice_addr is None:
            self._g_rollservice_addr = self._scan_for_g_rollservice()

        if self._g_rollservice_addr is None:
            logger.warning(
                "MysRollServiceDiscovery: g_rollServiceInstance not found. "
                "Has the user tapped the multiplier button at least once?"
            )
            return None

        return self._read_dice_via_chain(self._g_rollservice_addr)

    def invalidate_cache(self) -> None:
        """Force re-scan for g_rollServiceInstance on next call."""
        self._g_rollservice_addr = None

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _get_mys_base(self) -> Optional[int]:
        """Return the base address of libmys_payload.so, or None."""
        maps_path = f"/proc/{self._pid}/maps"
        try:
            with open(maps_path, "r", encoding="utf-8") as f:
                for line in f:
                    if self.MYS_LIB_NAME in line:
                        start = line.split("-", maxsplit=1)[0]
                        base = int(start, 16)
                        logger.debug(
                            "MysRollServiceDiscovery: %s base = 0x%x",
                            self.MYS_LIB_NAME,
                            base,
                        )
                        return base
        except OSError as exc:
            logger.error("MysRollServiceDiscovery: cannot read maps: %s", exc)
        return None

    def _read_ptr(self, addr: int) -> Optional[int]:
        """Read a 64-bit pointer from an absolute address."""
        try:
            os.lseek(self._mem_file, addr, os.SEEK_SET)
            data = os.read(self._mem_file, 8)
            if len(data) != 8:
                return None
            value = struct.unpack("<Q", data)[0]
            return value if value != 0 else None
        except OSError:
            return None

    def _read_int32(self, addr: int) -> Optional[int]:
        """Read a signed 32-bit integer from an absolute address."""
        try:
            os.lseek(self._mem_file, addr, os.SEEK_SET)
            data = os.read(self._mem_file, 4)
            if len(data) != 4:
                return None
            return struct.unpack("<i", data)[0]
        except OSError:
            return None

    def _is_plausible_dice(self, value: int) -> bool:
        return self._DICE_MIN <= value <= self._DICE_MAX

    def _scan_for_g_rollservice(self) -> Optional[int]:
        """Scan libmys_payload.so data segment for g_rollServiceInstance.

        Validates each candidate by following the full pointer chain and
        checking whether the resulting dice count is plausible.

        Returns:
            Absolute address of g_rollServiceInstance, or None.
        """
        assert self._mys_base is not None
        logger.debug("MysRollServiceDiscovery: scanning for g_rollServiceInstance …")

        for offset in range(0, self._SCAN_RANGE, self._SCAN_STEP):
            candidate_addr = self._mys_base + offset
            dice = self._read_dice_via_chain(candidate_addr)
            if dice is not None:
                logger.info(
                    "MysRollServiceDiscovery: g_rollServiceInstance found at "
                    "libmys+0x%x (abs 0x%x), dice=%d",
                    offset,
                    candidate_addr,
                    dice,
                )
                return candidate_addr

        return None

    def _read_dice_via_chain(self, g_ptr_addr: int) -> Optional[int]:
        """Follow the pointer chain from a g_rollServiceInstance candidate.

        Chain:
            ``*g_ptr_addr``                    →  RollService*
            ``*(RollService* + 0x10)``         →  RollModel*
            ``*(RollModel*   + 0x18)`` (i32)   →  dice count

        Returns:
            Dice count if the full chain resolves to a plausible value,
            otherwise None.
        """
        service_ptr = self._read_ptr(g_ptr_addr)
        if service_ptr is None:
            return None

        model_ptr = self._read_ptr(service_ptr + self._ROLLSERVICE_TO_SERVERMODEL)
        if model_ptr is None:
            return None

        dice = self._read_int32(model_ptr + self._ROLLMODEL_TO_DICE)
        if dice is None:
            return None

        return dice if self._is_plausible_dice(dice) else None
