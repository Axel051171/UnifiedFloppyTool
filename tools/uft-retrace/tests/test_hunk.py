import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

from uft_retrace.hunk import HUNK_BSS, HUNK_CODE, HUNK_END, HUNK_HEADER
from uft_retrace.hunk import HunkError, parse_hunk
from uft_retrace.inventory import safe_extract_zip


def words(*values):
    return b"".join(struct.pack(">I", value) for value in values)


class HunkTests(unittest.TestCase):
    def test_nonzero_first_hunk_uses_table_order(self):
        payload = b"\x4e\x75\x00\x00"
        image = (
            words(HUNK_HEADER, 0, 1, 7, 7, 0x40000001,
                  HUNK_CODE, 1)
            + payload
            + words(HUNK_END)
        )
        parsed = parse_hunk(image)
        self.assertEqual(parsed.first_hunk, 7)
        self.assertEqual(parsed.segments[0].payload, payload)
        self.assertEqual(parsed.segments[0].memory_flags, 0x40000000)

    def test_bss_and_trailing_bytes(self):
        image = words(HUNK_HEADER, 0, 1, 0, 0, 2, HUNK_BSS, 2, HUNK_END) + b"xx"
        parsed = parse_hunk(image)
        self.assertEqual(parsed.segments[0].size_bytes, 8)
        self.assertEqual(parsed.trailing_bytes, 2)

    def test_truncated_hunk_is_rejected(self):
        image = words(HUNK_HEADER, 0, 1, 0, 0, 1, HUNK_CODE, 2, 0x12345678)
        with self.assertRaises(HunkError):
            parse_hunk(image)


class SafeZipTests(unittest.TestCase):
    def test_path_traversal_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            archive = root / "bad.zip"
            with zipfile.ZipFile(archive, "w") as zf:
                zf.writestr("../outside.bin", b"no")
            with self.assertRaises(ValueError):
                safe_extract_zip(archive, root / "out")


if __name__ == "__main__":
    unittest.main()
