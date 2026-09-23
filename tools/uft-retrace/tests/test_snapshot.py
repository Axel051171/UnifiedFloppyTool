import unittest

from uft_retrace.snapshot import changed_ranges


class SnapshotTests(unittest.TestCase):
    def test_ranges_merge_small_equal_gap(self):
        before = bytes(40)
        after = bytearray(before)
        after[4:10] = b"abcdef"
        after[12:18] = b"ghijkl"
        self.assertEqual(
            changed_ranges(before, bytes(after), base=0x1000,
                           minimum_run=4, merge_gap=2),
            [{"file_offset": 4, "address": 0x1004, "length": 14}],
        )

    def test_short_noise_is_filtered(self):
        self.assertEqual(changed_ranges(b"abc", b"axc", minimum_run=2), [])

    def test_size_change_is_detected(self):
        result = changed_ranges(b"", b"1234", base=32, minimum_run=1)
        self.assertEqual(result[0], {"file_offset": 0, "address": 32, "length": 4})


if __name__ == "__main__":
    unittest.main()
