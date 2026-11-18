"""Exercise the assembled experimental speaker gates and their stock-call ABI."""

import itertools
from pathlib import Path
import re
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build
from pi32v2isa import objdump, table


class MuteGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            src = build.ROOT / "src" / "aprs_mute.s"
            cls.origin = build.IMAGE_BASE + build.CAVE
            cls.program = build.link([build.assemble(src, out)], cls.origin,
                                     "ml_mute_squelch", out, "mute")
        cls.code = build.text_of(cls.program, cls.origin)
        cls.decode = objdump.decoder_for(table.load(), mcpu="r3").decode

    def execute(self, entry, r0, r1, memory):
        regs = {"r0": r0, "r1": r1, "r2": 0, "r3": 0}
        stack = []
        pc = build.symbol(self.program, entry) - self.origin
        rets = -1

        def value(operand):
            return regs[operand] if operand.startswith("r") else int(operand, 0)

        for _ in range(40):
            _, _, step, op = self.decode(self.code, pc)
            pc += step
            if op == "[--sp] = rets":
                stack.append(rets)
            elif op == "{rets} = [sp++]":
                rets = stack.pop()
            elif op == "pc = [sp++]":
                self.assertEqual(stack, [-1])
                return None
            elif op.startswith("call "):
                rets, pc = pc, pc + int(op[5:])
            elif op == "rts":
                pc = rets
            elif op == "goto r3":
                self.assertEqual((stack, rets, regs["r0"], regs["r1"]), ([], -1, r0, r1))
                return regs["r3"]
            elif branch := re.fullmatch(r"if \((r\d) (==|!=) ([\w]+)\) goto (-?\d+)", op):
                left, operation, right, delta = branch.groups()
                if (value(left) == value(right)) == (operation == "=="):
                    pc += int(delta)
            elif load := re.fullmatch(r"(r\d) = b?\[(r\d)\+(\w+)\](?: \(u\))?", op):
                dst, base, offset = load.groups()
                regs[dst] = memory[value(base) + value(offset)]
            elif assign := re.fullmatch(r"(r\d) = (\d+)", op):
                regs[assign[1]] = int(assign[2])
            else:
                self.fail(f"Unexpected instruction {op!r}")
        self.fail("Speaker gate did not return")

    def test_opens_only_mute_the_selected_b_frequency(self):
        gates = (("ml_mute_squelch", 0x01E5234C), ("ml_mute_volume", 0x01E52328))
        for (entry, stock), enabled, side, current, freq_a, freq_b, opened in itertools.product(
            gates, (0, 1, 2), (0, 1, 2), (1, 2, 3), (1, 2), (1, 2), (0, 1)
        ):
            memory = {0xC0F0 + 944: enabled, 0xC0F0 + 1090: side,
                      0x11AD0 + 444: current, 0x11AD0 + 464: freq_a, 0x11AD0 + 472: freq_b}
            args = (4, opened) if entry == "ml_mute_squelch" else (opened, 7)
            muted = opened and enabled == 1 and side == 2 and current == freq_b and current != freq_a
            self.assertEqual(self.execute(entry, *args, memory), None if muted else stock)


if __name__ == "__main__":
    unittest.main()
