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
        cls.origin = build.IMAGE_BASE + build.CAVE
        cls.programs = {}
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            src = build.ROOT / "src" / "aprs_mute.s"
            for resident in (False, True):
                source = out / "gate.s"
                prefix = ".equ ML_RUNTIME_BOOT, 0x1ea9e9e\n" if resident else ""
                source.write_text(prefix + src.read_text())
                prog = build.link([build.assemble(source, out)], cls.origin,
                                  "ml_mute_squelch", out, "mute")
                cls.programs[resident] = (prog, build.text_of(prog, cls.origin))
        cls.decode = objdump.decoder_for(table.load(), mcpu="r3").decode

    def execute(self, entry, r0, r1, memory, resident):
        program, code = self.programs[resident]
        regs = {"r0": r0, "r1": r1, "r2": 0, "r3": 0}
        stack = []
        pc = build.symbol(program, entry) - self.origin
        rets = -1

        def value(operand):
            return regs[operand] if operand.startswith("r") else int(operand, 0)

        for _ in range(40):
            _, _, step, op = self.decode(code, pc)
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
        for (entry, stock), enabled, side, current, freq_a, freq_b, opened, preference in itertools.product(
            gates, (0, 1, 2), (0, 1, 2), (1, 2, 3), (1, 2), (1, 2), (0, 1), (None, 0, 1)
        ):
            memory = {0xC0F0 + 944: enabled, 0xC0F0 + 1090: side,
                      0x11AD0 + 444: current, 0x11AD0 + 464: freq_a,
                      0x11AD0 + 472: freq_b, build.RESIDENT_RAM + 4: preference}
            args = (4, opened) if entry == "ml_mute_squelch" else (opened, 7)
            muted = (preference != 0 and opened and enabled == 1 and side == 2
                     and current == freq_b and current != freq_a)
            self.assertEqual(self.execute(entry, *args, memory, preference is not None), None if muted else stock)


if __name__ == "__main__":
    unittest.main()
