#!/usr/bin/env python3
# match.py -- pair the functions of two Ghidra dumps (DumpFunctions.java) and report what changed.
#   python3 match.py <proto.tsv> <retail.tsv>
# Named functions (exports, demangled) pair by name. Unnamed ones (FUN_xxx) pair first by identical
# fingerprint (moved but unchanged), then by identical callee set (changed body, same role). Reports named
# functions whose fingerprint differs, then unpaired unnamed functions on either side with their callees.
import sys, collections
def load(p):
    rows = []
    for line in open(p):
        e, name, n, fp, cal = (line.rstrip('\n').split('\t') + [''])[:5]
        named = ';'.join(c for c in cal.split(';') if c and not c.startswith(('FUN_', 'thunk_FUN_')))
        rows.append(dict(entry=e, name=name, n=int(n), fp=fp, callees=named))
    return rows
A, B = load(sys.argv[1]), load(sys.argv[2])
unnamed = lambda r: r['name'].startswith(('FUN_', 'thunk_FUN_', 'Catch', 'Unwind'))
na = {r['name']: r for r in A if not unnamed(r)}; nb = {r['name']: r for r in B if not unnamed(r)}
changed = [(n, na[n]['n'], nb[n]['n']) for n in sorted(set(na) & set(nb)) if na[n]['fp'] != nb[n]['fp']]
print(f'# named: proto {len(na)} retail {len(nb)} common {len(set(na)&set(nb))} changed {len(changed)}')
for n, x, y in changed: print(f'  CHANGED {x:5d} -> {y:5d}  {n}')
for n in sorted(set(nb) - set(na)): print(f'  NEW     {nb[n]["n"]:5d}  {n}')
for n in sorted(set(na) - set(nb)): print(f'  GONE    {na[n]["n"]:5d}  {n}')
ua = [r for r in A if unnamed(r)]; ub = [r for r in B if unnamed(r)]
fpb = collections.Counter(r['fp'] for r in ub); fpa = collections.Counter(r['fp'] for r in ua)
ua2 = [r for r in ua if fpb[r['fp']] == 0]; ub2 = [r for r in ub if fpa[r['fp']] == 0]
cb = collections.defaultdict(list)
for r in ub2: cb[r['callees']].append(r)
pairs, left = [], []
for r in ua2:
    c = cb.get(r['callees'])
    if c and r['callees']:
        pairs.append((r, c.pop(0)))
    else: left.append(r)
right = [r for v in cb.values() for r in v]
print(f'# unnamed: proto {len(ua)} retail {len(ub)}; unchanged-by-fingerprint proto {len(ua)-len(ua2)}; '
      f'changed (paired by callees) {len(pairs)}; unpaired proto {len(left)} retail {len(right)}')
for a, b in pairs: print(f'  CHANGED {a["n"]:5d} -> {b["n"]:5d}  {a["entry"]}/{b["entry"]}  calls: {a["callees"][:160]}')
for r in left: print(f'  PROTO-ONLY {r["n"]:5d} {r["entry"]}  calls: {r["callees"][:160]}')
for r in right: print(f'  RETAIL-ONLY {r["n"]:5d} {r["entry"]}  calls: {r["callees"][:160]}')
