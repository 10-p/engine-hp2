#!/usr/bin/env python3
# layout-diff.py -- script property layouts, prototype packages vs retail packages, class by class.
#   python3 tools/dev/retail/layout-diff.py <proto System dir> <retail System dir> [Pkg ...]
# Uses tools/dev/layout/layout.py (HP2 433 UStruct::Link emulation). Reports every class whose (name, type,
# offset, size) list or PropertiesSize differs, and classes present on one side only. Bools packed into one
# word share an offset; a bool inserted into such a word changes no offset but shifts the bits after it, so
# the per-class list also compares the order of bools within each word.
import os, sys, importlib
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, '..', 'layout'))
P, R = sys.argv[1], sys.argv[2]
pk = sys.argv[3:] or ['Core', 'Engine', 'Fire', 'Editor']
def snap(sysdir):
    import layout
    importlib.reload(layout)
    layout.load([os.path.join(sysdir, f'{p}.u') for p in pk])
    out = {}
    for p in layout.pkgs:
        for e in p.exports:
            if e['cls'] == 'Class' and p.name.lower() in [x.lower() for x in pk]:
                try:
                    size = layout.props_size(e)
                except Exception as ex:
                    out[e['name']] = ('error', str(ex)); continue
                out[e['name']] = (size, layout.LAYOUT[(p.name, e['idx'])], p.name)
    return out
a, b = snap(P), snap(R)
for n in sorted(set(a) | set(b)):
    if n not in a: print(f'+ class {n} (retail only, {b[n][2]})'); continue
    if n not in b: print(f'- class {n} (prototype only)'); continue
    if a[n][:2] == b[n][:2]: continue
    sa, la = a[n][0], a[n][1]; sb, lb = b[n][0], b[n][1]
    na = [x[0] for x in la]; nb = [x[0] for x in lb]
    added = [x for x in lb if x[0] not in na]; removed = [x for x in la if x[0] not in nb]
    moved = [(x[0], dict((y[0], y[2]) for y in la)[x[0]], x[2]) for x in lb if x[0] in na and dict((y[0], y[2]) for y in la)[x[0]] != x[2]]
    print(f'~ {n}: size {sa} -> {sb}; own props +{[(x[0],x[1],x[2]) for x in added]} -{[x[0] for x in removed]}; moved {len(moved)}')
