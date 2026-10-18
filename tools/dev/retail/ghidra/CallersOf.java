// CallersOf.java -- Ghidra headless post-script: for each given address (an export stub or a body), list the
// functions that reference it (calls through the stub or the body), one "<target>\t<caller-entry>\t<caller>" line.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.io.*;
import java.util.*;
public class CallersOf extends GhidraScript {
	@Override
	public void run() throws Exception {
		String[] a = getScriptArgs();
		try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
			for (int i = 1; i < a.length; i++) {
				Set<String> seen = new TreeSet<>();
				for (Reference r : getReferencesTo(toAddr(a[i]))) {
					Function c = getFunctionContaining(r.getFromAddress());
					if (c != null) seen.add(c.getEntryPoint() + "\t" + c.getName(true));
				}
				for (String s : seen) w.println(a[i] + "\t" + s);
			}
		}
	}
}
