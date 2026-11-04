// Seed entry points in the offline ARM APRS image for decompilation.
// @category TD-H9

import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class H9AprsSeed extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("Expected report path and ARM addresses");
        }
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            for (int i = 1; i < args.length; i++) {
                Address address = toAddr(Long.decode(args[i]));
                try {
                    boolean decoded = disassemble(address);
                    Function function = currentProgram.getFunctionManager()
                        .getFunctionContaining(address);
                    if (function == null) function = createFunction(address, null);
                    out.printf("%s disassembled=%s function=%s%n", address,
                        decoded, function == null ? "none" : function.getEntryPoint());
                } catch (Exception error) {
                    out.printf("%s error=%s%n", address, error);
                }
            }
            analyzeChanges(currentProgram);
        }
    }
}
