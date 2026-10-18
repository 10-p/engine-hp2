// DecompileAt.java -- Ghidra headless post-script: decompile the functions at the given entry addresses.
//   analyzeHeadless <proj-dir> <proj> -process <file> -noanalysis -postScript DecompileAt.java <out-dir> <addr> ...
// Writes <out-dir>/<addr>.c per function (the decompiler's C), for side-by-side diffing of two builds.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.Function;
import java.io.*;

public class DecompileAt extends GhidraScript {
	@Override
	public void run() throws Exception {
		String[] a = getScriptArgs();
		File dir = new File(a[0]); dir.mkdirs();
		DecompInterface d = new DecompInterface();
		d.openProgram(currentProgram);
		for (int i = 1; i < a.length; i++) {
			Function f = getFunctionAt(toAddr(a[i]));
			if (f == null) f = getFunctionContaining(toAddr(a[i]));
			String text = "// no function at " + a[i];
			// An incremental-link stub (a single JMP) names the export; decompile the body it jumps to.
			if (f != null && f.getBody().getNumAddresses() <= 5) {
				ghidra.program.model.listing.Instruction ins = getInstructionAt(f.getEntryPoint());
				if (ins != null && ins.getMnemonicString().startsWith("JMP") && ins.getFlows().length == 1) {
					Function t = getFunctionAt(ins.getFlows()[0]);
					if (t == null) t = createFunction(ins.getFlows()[0], null);
					if (t != null) { text = "// stub " + f.getName(true) + " -> " + t.getEntryPoint() + "\n"; f = t; }
				}
			}
			if (f != null) {
				DecompileResults r = d.decompileFunction(f, 120, monitor);
				text = (text.startsWith("// stub") ? text : "") + "// " + f.getName(true) + " @ " + f.getEntryPoint() + "\n" +
					(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed: " + r.getErrorMessage());
			}
			try (PrintWriter w = new PrintWriter(new FileWriter(new File(dir, a[i] + ".c")))) { w.print(text); }
		}
	}
}
