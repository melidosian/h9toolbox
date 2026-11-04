// Export references to selected addresses from an analyzed project.
// Headless arguments: output-path address [address ...]. Read-only script.
// @category TD-H9

import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class ExportReferences extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2)
            throw new IllegalArgumentException("Expected output path and address");
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            out.println("Program: " + currentProgram.getName());
            out.println("Language: " + currentProgram.getLanguageID());
            for (int i = 1; i < args.length; i++) {
                Address target = toAddr(args[i]);
                out.println("\n=== " + args[i] + " ===");
                ReferenceIterator refs = currentProgram.getReferenceManager()
                    .getReferencesTo(target);
                int count = 0;
                while (refs.hasNext()) {
                    Reference ref = refs.next();
                    Address from = ref.getFromAddress();
                    Function function = currentProgram.getFunctionManager()
                        .getFunctionContaining(from);
                    Instruction instruction = currentProgram.getListing()
                        .getInstructionContaining(from);
                    out.println(from + " " + ref.getReferenceType() +
                        " function=" + (function == null ? "none" :
                        function.getName()) + " instruction=" +
                        (instruction == null ? "none" : instruction));
                    count++;
                }
                out.println("count=" + count);
            }
        }
    }
}
