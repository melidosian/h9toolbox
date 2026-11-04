// Find code functions using SLIP-like APRS framing byte constants.
// @category TD-H9

import java.io.FileWriter;
import java.io.PrintWriter;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;

public class H9FrameCandidates extends GhidraScript {
    private static final int[] VALUES = {0xc0, 0xdb, 0xdc, 0xdd, 0x10, 0x11, 0x13, 0x20};

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Output path required");
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext()) {
                Function fn = functions.next();
                int[] hits = new int[VALUES.length];
                InstructionIterator instructions = currentProgram.getListing().getInstructions(fn.getBody(), true);
                while (instructions.hasNext()) {
                    Instruction instruction = instructions.next();
                    for (int op = 0; op < instruction.getNumOperands(); op++) {
                        for (Object object : instruction.getOpObjects(op)) {
                            if (!(object instanceof Scalar)) continue;
                            long value = ((Scalar)object).getUnsignedValue();
                            for (int k = 0; k < VALUES.length; k++) {
                                if (value == VALUES[k]) hits[k]++;
                            }
                        }
                    }
                }
                if (hits[0] > 0 && (hits[1] > 0 || hits[2] > 0 || hits[3] > 0)) {
                    out.printf("function=%s bytes=%d c0=%d db=%d dc=%d dd=%d cmd10=%d cmd11=%d cmd13=%d cmd20=%d%n",
                        fn.getEntryPoint(), fn.getBody().getNumAddresses(), hits[0], hits[1], hits[2],
                        hits[3], hits[4], hits[5], hits[6], hits[7]);
                }
            }
        }
    }
}
