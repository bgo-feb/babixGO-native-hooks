"""Unit tests for the Python memory reader MVP."""

import struct
import unittest
from unittest.mock import MagicMock
from unittest.mock import mock_open
from unittest.mock import patch

from core.memory_reader import IL2CPPMemoryReader


class TestMemoryReaderInit(unittest.TestCase):
    """Tests for reader initialization and process discovery."""

    def test_init_defaults(self) -> None:
        """Default constructor values should be populated."""
        reader = IL2CPPMemoryReader()

        self.assertEqual(reader.package_name, "com.scopely.monopolygo")
        self.assertEqual(reader.lib_name, "libil2cpp.so")
        self.assertIsNone(reader.dice_line_item_address)
        self.assertIsNone(reader.pid)
        self.assertIsNone(reader.base_address)
        self.assertIsNone(reader.mem_file)

    @patch("subprocess.run")
    def test_find_process_id_success(self, mock_run: MagicMock) -> None:
        """pidof output should be parsed correctly."""
        mock_run.return_value = MagicMock(returncode=0, stdout="12345\n")

        reader = IL2CPPMemoryReader()

        self.assertEqual(reader._find_process_id(), 12345)
        mock_run.assert_called_once()

    @patch("subprocess.run")
    def test_find_process_id_ps_fallback_success(self, mock_run: MagicMock) -> None:
        """ps should be used when pidof does not find the process."""
        mock_run.side_effect = [
            MagicMock(returncode=1, stdout=""),
            MagicMock(
                returncode=0,
                stdout=(
                    "USER PID PPID VSZ RSS WCHAN ADDR S NAME\n"
                    "u0_a123 23456 1 0 0 0 0 S com.scopely.monopolygo\n"
                ),
            ),
        ]

        reader = IL2CPPMemoryReader()

        self.assertEqual(reader._find_process_id(), 23456)
        self.assertEqual(mock_run.call_count, 2)

    @patch("subprocess.run")
    def test_find_process_id_not_found(self, mock_run: MagicMock) -> None:
        """None should be returned when neither pidof nor ps find the process."""
        mock_run.side_effect = [
            MagicMock(returncode=1, stdout=""),
            MagicMock(
                returncode=0,
                stdout="USER PID NAME\nu0_a124 54321 com.example.other\n",
            ),
        ]

        reader = IL2CPPMemoryReader()

        self.assertIsNone(reader._find_process_id())

    def test_parse_maps_file(self) -> None:
        """The first matching library mapping should define the base address."""
        maps_content = (
            "7000000000-7000001000 r--p 00000000 fd:00 12345 /system/lib64/libc.so\n"
            "7123456000-7123800000 r-xp 00000000 fd:00 67890 "
            "/data/app/com.scopely.monopolygo/lib/arm64/libil2cpp.so\n"
            "7123800000-7123900000 rw-p 00000000 fd:00 67890 "
            "/data/app/com.scopely.monopolygo/lib/arm64/libil2cpp.so\n"
        )

        with patch("builtins.open", mock_open(read_data=maps_content)):
            reader = IL2CPPMemoryReader()
            reader.pid = 12345

            self.assertEqual(reader._get_library_base_address(), 0x7123456000)

    @patch("os.open")
    @patch.object(IL2CPPMemoryReader, "_get_library_base_address")
    @patch.object(IL2CPPMemoryReader, "_find_process_id")
    def test_initialize_success(
        self,
        mock_pid: MagicMock,
        mock_base: MagicMock,
        mock_open_fd: MagicMock,
    ) -> None:
        """Successful initialization should populate all reader state."""
        mock_pid.return_value = 12345
        mock_base.return_value = 0x7000000000
        mock_open_fd.return_value = 99

        reader = IL2CPPMemoryReader()

        self.assertTrue(reader.initialize())
        self.assertEqual(reader.pid, 12345)
        self.assertEqual(reader.base_address, 0x7000000000)
        self.assertEqual(reader.mem_file, 99)

    @patch.object(IL2CPPMemoryReader, "_find_process_id")
    def test_initialize_failure_when_process_missing(self, mock_pid: MagicMock) -> None:
        """Initialization should fail cleanly when the process is missing."""
        mock_pid.return_value = None

        reader = IL2CPPMemoryReader()

        self.assertFalse(reader.initialize())
        self.assertIsNone(reader.base_address)
        self.assertIsNone(reader.mem_file)

    @patch("os.open")
    @patch.object(IL2CPPMemoryReader, "_get_library_base_address")
    @patch.object(IL2CPPMemoryReader, "_find_process_id")
    def test_initialize_failure_when_mem_open_fails(
        self,
        mock_pid: MagicMock,
        mock_base: MagicMock,
        mock_open_fd: MagicMock,
    ) -> None:
        """Initialization should fail when `/proc/<pid>/mem` cannot be opened."""
        mock_pid.return_value = 12345
        mock_base.return_value = 0x7000000000
        mock_open_fd.side_effect = OSError("permission denied")

        reader = IL2CPPMemoryReader()

        self.assertFalse(reader.initialize())
        self.assertEqual(reader.pid, 12345)
        self.assertEqual(reader.base_address, 0x7000000000)
        self.assertIsNone(reader.mem_file)


class TestMemoryReading(unittest.TestCase):
    """Tests for low-level memory reads and dice access."""

    def setUp(self) -> None:
        """Create a ready-to-read reader instance."""
        self.reader = IL2CPPMemoryReader(dice_line_item_address=0x7FFF0000)
        self.reader.pid = 12345
        self.reader.base_address = 0x7000000000
        self.reader.mem_file = 99

    @patch("os.lseek")
    @patch("os.read")
    def test_read_int32(self, mock_read: MagicMock, mock_lseek: MagicMock) -> None:
        """Base-relative int32 reads should decode little-endian data."""
        mock_read.return_value = b"\x2a\x00\x00\x00"

        value = self.reader.read_memory(0x100, 4, "int32")

        self.assertEqual(value, 42)
        mock_lseek.assert_called_once_with(99, 0x7000000100, 0)
        mock_read.assert_called_once_with(99, 4)

    @patch("os.lseek")
    @patch("os.read")
    def test_read_int64(self, mock_read: MagicMock, mock_lseek: MagicMock) -> None:
        """Base-relative int64 reads should decode correctly."""
        mock_read.return_value = struct.pack("<q", 9001)

        value = self.reader.read_memory(0x200, 8, "int64")

        self.assertEqual(value, 9001)
        mock_lseek.assert_called_once_with(99, 0x7000000200, 0)

    @patch("os.lseek")
    @patch("os.read")
    def test_read_float(self, mock_read: MagicMock, _mock_lseek: MagicMock) -> None:
        """Base-relative float reads should decode correctly."""
        mock_read.return_value = struct.pack("<f", 3.5)

        value = self.reader.read_memory(0x300, 4, "float")

        self.assertAlmostEqual(value, 3.5, places=5)

    @patch("os.lseek")
    @patch("os.read")
    def test_read_raw(self, mock_read: MagicMock, _mock_lseek: MagicMock) -> None:
        """Raw reads should return the unmodified bytes."""
        mock_read.return_value = b"\xaa\xbb\xcc\xdd"

        value = self.reader.read_memory(0x400, 4, "raw")

        self.assertEqual(value, b"\xaa\xbb\xcc\xdd")

    @patch("os.lseek")
    @patch("os.read")
    def test_read_short_read_returns_none(
        self,
        mock_read: MagicMock,
        _mock_lseek: MagicMock,
    ) -> None:
        """Short reads should return None instead of raising."""
        mock_read.return_value = b"\x01\x02"

        self.assertIsNone(self.reader.read_memory(0x500, 4, "int32"))

    @patch("os.lseek")
    @patch("os.read")
    def test_read_struct_error_returns_none(
        self,
        mock_read: MagicMock,
        _mock_lseek: MagicMock,
    ) -> None:
        """Decode failures should return None."""
        mock_read.return_value = b"\x01\x02\x03"

        self.assertIsNone(self.reader.read_memory(0x600, 3, "int32"))

    @patch("os.lseek")
    @patch("os.read")
    def test_get_dice_count(self, mock_read: MagicMock, mock_lseek: MagicMock) -> None:
        """Dice count should read `_count` from the manual line-item pointer."""
        mock_read.return_value = struct.pack("<q", 150)

        dice = self.reader.get_dice_count()

        self.assertEqual(dice, 150)
        mock_lseek.assert_called_once_with(
            99,
            0x7FFF0000 + IL2CPPMemoryReader.DICE_LINE_ITEM_COUNT_OFFSET,
            0,
        )
        mock_read.assert_called_once_with(99, 8)

    @patch("os.read")
    @patch("os.lseek")
    def test_get_dice_count_without_anchor(
        self,
        mock_lseek: MagicMock,
        mock_read: MagicMock,
    ) -> None:
        """Dice reads should fail cleanly when the manual anchor is missing."""
        self.reader.dice_line_item_address = None

        self.assertIsNone(self.reader.get_dice_count())
        mock_lseek.assert_not_called()
        mock_read.assert_not_called()

    def test_is_valid(self) -> None:
        """Reader validity should depend on PID, base, and mem FD."""
        self.assertTrue(self.reader.is_valid())

        self.reader.pid = None
        self.assertFalse(self.reader.is_valid())

    @patch("os.close")
    def test_close(self, mock_close: MagicMock) -> None:
        """close() should close the FD and clear the stored handle."""
        self.reader.close()

        mock_close.assert_called_once_with(99)
        self.assertIsNone(self.reader.mem_file)


if __name__ == "__main__":
    unittest.main()
