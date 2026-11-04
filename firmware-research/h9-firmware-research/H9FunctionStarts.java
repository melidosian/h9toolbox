// Export analyzed Pi32 function entrypoints as seeds for the independent decoder.
// @category TD-H9

import java.io.FileWriter;
import java.io.PrintWriter;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class H9FunctionStarts extends GhidraScript {
    private static final long BASE = 0x01e00100L;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Expected output path");
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext()) {
                Function fn = functions.next();
                long offset = fn.getEntryPoint().getOffset() - BASE;
                if (offset >= 0) out.printf("0x%x%n", offset);
            }
        }
    }
}
