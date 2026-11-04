// Find references to addresses in either TD-H9 Ghidra project.
// @category TD-H9

import java.io.FileWriter;
import java.io.PrintWriter;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class H9Refs extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) throw new IllegalArgumentException("Output path and addresses required");
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            for (int i = 1; i < args.length; i++) {
                Address target = toAddr(Long.decode(args[i]));
                out.println("target=" + target);
                ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(target);
                while (refs.hasNext()) {
                    Reference ref = refs.next();
                    Address source = ref.getFromAddress();
                    Function fn = currentProgram.getFunctionManager().getFunctionContaining(source);
                    out.printf("  source=%s type=%s function=%s instruction=%s%n",
                        source, ref.getReferenceType(), fn == null ? "none" : fn.getEntryPoint(),
                        currentProgram.getListing().getInstructionContaining(source));
                }
            }
        }
    }
}
