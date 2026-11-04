// Search decompiled functions for selected text when RAM has no references.
// Headless arguments: output-path search-text. Read-only script.
// @category TD-H9

import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class FindDecompilerText extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2)
            throw new IllegalArgumentException("Expected output path and text");
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram))
            throw new IllegalStateException("Could not open decompiler");
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            out.println("Program: " + currentProgram.getName());
            out.println("Search: " + args[1]);
            FunctionIterator functions = currentProgram.getFunctionManager()
                .getFunctions(true);
            int seen = 0;
            int matched = 0;
            int failed = 0;
            while (functions.hasNext() && !monitor.isCancelled()) {
                Function function = functions.next();
                DecompileResults result = decompiler.decompileFunction(
                    function, 15, monitor);
                seen++;
                if (!result.decompileCompleted()) {
                    failed++;
                    continue;
                }
                String body = result.getDecompiledFunction().getC();
                if (body.contains(args[1])) {
                    out.println("\n=== " + function.getEntryPoint() + " " +
                                function.getName() + " ===");
                    out.println(body);
                    matched++;
                }
            }
            out.println("\nseen=" + seen + " matched=" + matched +
                        " failed=" + failed);
        } finally {
            decompiler.dispose();
        }
    }
}
