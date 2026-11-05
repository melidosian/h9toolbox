"""Check that exception entry is included in the APRS RAM estimate."""

import importlib.util
from pathlib import Path
import unittest


MODULE = Path(__file__).resolve().parents[1] / "analyze_aprs_stack.py"
SPEC = importlib.util.spec_from_file_location("analyze_aprs_stack", MODULE)
assert SPEC and SPEC.loader
stack = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(stack)


class AprsStackTests(unittest.TestCase):
    def test_one_interrupt_can_exceed_headroom_without_a_long_direct_path(self):
        disassembly = "\n".join((
            "00000000 <h9_aprs_boot>:",
            "   0:  f000 f800  bl  8 <h9_aprs_board_main>",
            "00000008 <h9_aprs_board_main>:",
            "00000010 <h9_aprs_adc1_handler>:",
        ))
        usage = [
            {"function": "h9_aprs_boot", "frame_bytes": 10},
            {"function": "h9_aprs_board_main", "frame_bytes": 40},
            {"function": "h9_aprs_adc1_handler", "frame_bytes": 100},
        ]
        report = stack.analyze(disassembly, usage, 180)
        self.assertFalse(report["largest_known_path_exceeds_headroom"])
        self.assertEqual(report["one_irq_preemption"]["thread_path_bytes"], 50)
        self.assertEqual(report["one_irq_preemption"]["thread_path_aligned_bytes"], 56)
        self.assertEqual(report["one_irq_preemption"]["total_bytes"], 192)
        self.assertTrue(report["one_irq_preemption"]["exceeds_headroom"])
        report = stack.analyze(disassembly, usage, 192)
        self.assertFalse(report["one_irq_preemption"]["exceeds_headroom"])


if __name__ == "__main__":
    unittest.main()
