// Read-only Ghidra script for the separate APRS-1.0.15 ARM image.
// @category TD-H9

import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class H9AprsInventory extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("Expected output report path and at least one address");
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            out.println("program=" + currentProgram.getName());
            out.println("language=" + currentProgram.getLanguageID());
            for (int i = 1; i < args.length; i++) {
                long value = Long.decode(args[i]);
                Address address = toAddr(value);
                Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
                out.printf("%naddress=%s function=%s%n", address,
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
                    out.println(code.substring(0, Math.min(code.length(), 16000)));
                } else {
                    out.println("decompile_error=" + result.getErrorMessage());
                }
            }
        } finally {
            decompiler.dispose();
        }
    }
}
