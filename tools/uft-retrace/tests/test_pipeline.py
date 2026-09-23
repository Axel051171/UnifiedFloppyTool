import json
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

from uft_retrace.analyze import analyze_workspace
from uft_retrace.hunk import HUNK_CODE, HUNK_END, HUNK_HEADER
from uft_retrace.inventory import create_inventory
from uft_retrace.report import render_report


class PipelineTests(unittest.TestCase):
    def test_inventory_analysis_report(self):
        pack = lambda *values: b"".join(struct.pack(">I", x) for x in values)
        executable = (pack(HUNK_HEADER, 0, 1, 0, 0, 1, HUNK_CODE, 1)
                      + b"\x4e\x75\x00\x00" + pack(HUNK_END))
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            archive = root / "input.zip"
            with zipfile.ZipFile(archive, "w") as zf:
                zf.writestr("tool", executable)
            work = root / "work"
            inventory = create_inventory(archive, work)
            analysis = analyze_workspace(work)
            report = render_report(work)
            self.assertTrue(inventory["files"][0]["is_hunk"])
            self.assertEqual(analysis["hunk_files"], ["tool"])
            self.assertIn("Clean-Room-Übergabe", report)
            json.loads((work / "inventory.json").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
