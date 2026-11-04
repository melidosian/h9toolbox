// Read-only Ghidra script for the stock TD-H9 v1.0.38 Pi32 application.
// @category TD-H9

import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class H9Inventory extends GhidraScript {
    private static final long BASE = 0x01e00100L;
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("Expected output report path and at least one app offset");
        }
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            out.println("program=" + currentProgram.getName());
            out.println("language=" + currentProgram.getLanguageID());
            out.println("image_base=" + currentProgram.getImageBase());
            int functions = 0;
            FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
            while (iterator.hasNext()) {
                iterator.next();
                functions++;
            }
            out.println("function_count=" + functions);

            DecompInterface decompiler = new DecompInterface();
            decompiler.openProgram(currentProgram);
            try {
                for (int i = 1; i < args.length; i++) {
                    long offset = Long.decode(args[i]);
                    Address address = toAddr(BASE + offset);
                    Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
                    out.printf("%napp+0x%06x address=%s function=%s%n", offset, address,
                        function == null ? "none" : function.getName() + "@" + function.getEntryPoint());
                    if (function == null) {
                        out.println("instruction=" + currentProgram.getListing().getInstructionAt(address));
                        continue;
                    }
                    ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(function.getEntryPoint());
                    int shown = 0;
                    while (refs.hasNext() && shown < 32) {
                        Reference ref = refs.next();
                        out.println("caller_reference=" + ref.getFromAddress() + " " + ref.getReferenceType());
                        shown++;
                    }
                    DecompileResults result = decompiler.decompileFunction(function, 20, monitor);
                    if (result.decompileCompleted() && result.getDecompiledFunction() != null) {
                        String code = result.getDecompiledFunction().getC();
                        out.println(code.substring(0, Math.min(code.length(), 12000)));
                    } else {
                        out.println("decompile_error=" + result.getErrorMessage());
                    }
                }
            } finally {
                decompiler.dispose();
            }
        }
    }
}
