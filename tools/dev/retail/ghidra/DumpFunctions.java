// DumpFunctions.java -- Ghidra headless post-script: one line per function, for build-to-build diffing.
//   analyzeHeadless <proj-dir> <proj> -import <dll> -postScript DumpFunctions.java <out.tsv> -scriptPath <dir>
// Columns: entry  name  instructions  fingerprint  callees(sorted, ';')
// fingerprint = sha1 over the normalised instruction stream: mnemonic + operand shape (registers kept;
// scalars, addresses and branch targets masked), so a function that only moved normalises identically.
// callees = names of the functions it calls (thunks resolved), the anchor that matches unnamed helpers.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.lang.OperandType;
import java.io.*;
import java.security.MessageDigest;
import java.util.*;

public class DumpFunctions extends GhidraScript {
	@Override
	public void run() throws Exception {
		String out = getScriptArgs()[0];
		try (PrintWriter w = new PrintWriter(new FileWriter(out))) {
			Listing listing = currentProgram.getListing();
			FunctionIterator it = listing.getFunctions(true);
			while (it.hasNext() && !monitor.isCancelled()) {
				Function f = it.next();
				if (f.isThunk() || f.isExternal()) continue;
				MessageDigest md = MessageDigest.getInstance("SHA-1");
				int n = 0;
				InstructionIterator ii = listing.getInstructions(f.getBody(), true);
				while (ii.hasNext()) {
					Instruction ins = ii.next();
					StringBuilder sb = new StringBuilder(ins.getMnemonicString());
					for (int i = 0; i < ins.getNumOperands(); i++) {
						int t = ins.getOperandType(i);
						if ((t & OperandType.REGISTER) != 0 && (t & (OperandType.ADDRESS | OperandType.SCALAR)) == 0)
							sb.append(' ').append(ins.getDefaultOperandRepresentation(i));
						else if ((t & OperandType.DYNAMIC) != 0)
							sb.append(" M").append(ins.getDefaultOperandRepresentation(i).replaceAll("0x[0-9a-fA-F]+", "N"));
						else sb.append(" X");
					}
					md.update(sb.toString().getBytes());
					md.update((byte) '\n');
					n++;
				}
				TreeSet<String> callees = new TreeSet<>();
				for (Function c : f.getCalledFunctions(monitor)) {
					Function r = c.isThunk() ? c.getThunkedFunction(true) : c;
					callees.add(r == null ? c.getName() : r.getName(true));
				}
				StringBuilder fp = new StringBuilder();
				for (byte b : md.digest()) fp.append(String.format("%02x", b));
				w.println(f.getEntryPoint() + "\t" + f.getName(true) + "\t" + n + "\t" + fp + "\t" + String.join(";", callees));
			}
		}
	}
}
