import json
import tempfile
import unittest
from pathlib import Path

from uft_retrace.trace import load_trace, summarize_trace


class TraceTests(unittest.TestCase):
    def test_jsonl_register_and_handoff_summary(self):
        rows = [
            {"seq": 1, "cycle": 10, "pc": "0x1000", "type": "mmio_write",
             "address": "0xdff024", "size": 2, "value": "0x8000"},
            {"seq": 2, "cycle": 20, "pc": "0x21000", "type": "execute"},
        ]
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "trace.jsonl"
            path.write_text("\n".join(json.dumps(row) for row in rows) + "\n",
                            encoding="utf-8")
            summary = summarize_trace(path)
            self.assertEqual(summary["event_count"], 2)
            self.assertEqual(summary["register_accesses"], {"DSKLEN": 1})
            self.assertEqual(summary["handoff_candidates"][0]["to_pc"], 0x21000)

    def test_csv_numbers_are_normalized(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "trace.csv"
            path.write_text("seq,cycle,pc,type,address,value\n"
                            "1,2,0x100,mmio_read,0xdff01a,0x4489\n",
                            encoding="utf-8")
            event = load_trace(path)[0]
            self.assertEqual(event["register"], "DSKBYTR")
            self.assertEqual(event["value"], 0x4489)


if __name__ == "__main__":
    unittest.main()
