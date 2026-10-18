#!/usr/bin/env python3
# func-diff.py -- which exported functions changed between two builds of the same Win32 DLL (prototype vs retail).
#   python3 tools/dev/retail/func-diff.py <proto.dll> <retail.dll>
# Disassembles both with i686-w64-mingw32-objdump, cuts each export from its address to the next export or
# function start, normalises every instruction (mnemonic + operand shape: immediates, absolute addresses and
# branch targets masked, registers kept) and reports exports whose normalised bodies differ. Same compiler,
# same source except for the edits => an unchanged function normalises identically even though every address
# moved. Data exports (vtables, statics) are skipped.
import re, subprocess, sys, hashlib
def exports(dll):
    out = subprocess.run(['i686-w64-mingw32-objdump', '-p', dll], capture_output=True, text=True).stdout
    base = int(re.search(r'ImageBase\s+([0-9a-f]+)', out).group(1), 16)
    eat, names = {}, {}
    part = out.split('Export Address Table -- Ordinal Base')[1]
    eat_part, name_part = part.split('[Ordinal/Name Pointer] Table')
    for m in re.finditer(r'\[\s*\d+\]\s+\+base\[\s*(\d+)\]\s+([0-9a-f]+)\s+Export RVA', eat_part):
        eat[int(m.group(1))] = base + int(m.group(2), 16)
    for m in re.finditer(r'\[\s*\d+\]\s+\+base\[\s*(\d+)\]\s+[0-9a-f]+\s+(\S+)', name_part):
        names[int(m.group(1))] = m.group(2)
    return {n: eat[o] for o, n in names.items() if o in eat}
def disasm(dll):
    out = subprocess.run(['i686-w64-mingw32-objdump', '-d', '--no-show-raw-insn', '-M', 'intel', dll], capture_output=True, text=True).stdout
    ins = []
    for line in out.splitlines():
        m = re.match(r'\s*([0-9a-f]+):\s+(.*)$', line)
        if m: ins.append((int(m.group(1), 16), m.group(2).strip()))
    return ins
def norm(t):
    t = re.sub(r'<[^>]*>', '', t)
    t = re.sub(r'0x[0-9a-f]+', 'N', t)
    t = re.sub(r'\b[0-9a-f]{5,}\b', 'A', t)
    t = re.sub(r'\b\d+\b', 'N', t)
    return ' '.join(t.split())
def bodies(dll):
    import bisect
    ex = exports(dll); ins = disasm(dll)
    idx = {a: i for i, (a, _) in enumerate(ins)}
    def real(a):  # follow an incremental-link thunk (jmp <body>) to the function body
        m = re.match(r'jmp\s+([0-9a-f]+)', ins[idx[a]][1]) if a in idx else None
        return int(m.group(1), 16) if m and int(m.group(1), 16) in idx else a
    starts = sorted({real(a) for a in ex.values() if a in idx})
    res = {}
    for name, a in ex.items():
        if a not in idx: continue  # data export
        a = real(a); i = idx[a]
        k = bisect.bisect_right(starts, a); end = starts[k] if k < len(starts) else float('inf')
        body = []
        while i < len(ins) and ins[i][0] < end and len(body) < 20000:
            body.append(norm(ins[i][1])); i += 1
        while body and body[-1] in ('int3', 'nop'): body.pop()
        res[name] = body
    return res
a, b = bodies(sys.argv[1]), bodies(sys.argv[2])
changed = []
for n in sorted(set(a) & set(b)):
    if a[n] != b[n]:
        changed.append((n, len(a[n]), len(b[n])))
print(f'# exported functions: proto {len(a)} retail {len(b)} common {len(set(a)&set(b))} changed {len(changed)}')
for n, la, lb in changed: print(f'{la:6d} -> {lb:6d}  {n}')
