// Export selected analyzed functions from a Ghidra project for review.
// Headless arguments: output-path address [address ...]. Read-only script.
// @category TD-H9

import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class ExportFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] arguments = getScriptArgs();
        if (arguments.length < 2) {
            throw new IllegalArgumentException("Expected output path and hex address");
        }
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("Could not open Ghidra decompiler");
        }
        try (PrintWriter out = new PrintWriter(new FileWriter(arguments[0]))) {
            out.println("Program: " + currentProgram.getName());
            out.println("Language: " + currentProgram.getLanguageID());
            for (int index = 1; index < arguments.length; index++) {
                Address address = toAddr(arguments[index]);
                Function function = currentProgram.getFunctionManager()
                    .getFunctionContaining(address);
                out.println("\n=== " + arguments[index] + " ===");
                if (function == null) {
                    out.println("No analyzed function contains this address.");
                    continue;
                }
                out.println("Function: " + function.getName() + " at " +
                            function.getEntryPoint());
                DecompileResults result = decompiler.decompileFunction(
                    function, 120, monitor);
                if (result.decompileCompleted()) {
                    out.println("\nPseudocode:");
                    out.println(result.getDecompiledFunction().getC());
                } else {
                    out.println("Decompilation failed: " + result.getErrorMessage());
                }
                out.println("\nInstructions:");
                InstructionIterator instructions = currentProgram.getListing()
                    .getInstructions(function.getBody(), true);
                while (instructions.hasNext()) {
                    Instruction instruction = instructions.next();
                    out.println(instruction.getAddress() + "  " + instruction);
                }
            }
        } finally {
            decompiler.dispose();
        }
    }
}
