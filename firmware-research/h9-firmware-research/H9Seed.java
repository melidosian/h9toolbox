// Seed observed TD-H9 Pi32 entry points in an offline Ghidra project.
// @category TD-H9

import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class H9Seed extends GhidraScript {
    private static final long BASE = 0x01e00100L;
    private static final long[] OFFSETS = {
        0x00000L, 0x42ce0L, 0x42de0L, 0x42e02L, 0x42e24L,
        0x4314cL, 0x43434L, 0x43598L, 0x43764L,
        0x4b8ceL, 0x60f7eL, 0x8a840L
    };

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            throw new IllegalArgumentException("Expected output report path and optional app offsets");
        }
        long[] offsets = OFFSETS;
        if (args.length > 1) {
            offsets = new long[args.length - 1];
            for (int i = 1; i < args.length; i++) offsets[i - 1] = Long.decode(args[i]);
        }
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            for (long offset : offsets) {
                Address address = toAddr(BASE + offset);
                try {
                    boolean decoded = disassemble(address);
                    Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
                    if (function == null) {
                        function = createFunction(address, null);
                    }
                    out.printf("app+0x%06x %s disassembled=%s function=%s%n",
                        offset, address, decoded,
                        function == null ? "none" : function.getEntryPoint());
                } catch (Exception error) {
                    out.printf("app+0x%06x %s error=%s%n", offset, address, error);
                }
            }
            analyzeChanges(currentProgram);
        }
    }
}
