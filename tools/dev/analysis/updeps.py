#!/usr/bin/env python3
"""List top-level package imports of UE1 package files (v61-v68+ header layout)."""
import struct, sys, os
def ci(b, o):
    x = b[o]; o += 1
    neg = x & 0x80; more = x & 0x40; v = x & 0x3f; shift = 6
    while more:
        x = b[o]; o += 1
        v |= (x & 0x7f) << shift; more = x & 0x80; shift += 7
        if shift > 27: break
    return (-v if neg else v), o
def parse(path):
    b = open(path,'rb').read()
    tag, ver, lic, flags, nc, no, ec, eo, ic, io = struct.unpack_from('<IHHIiiiiii', b, 0)
    if tag != 0x9E2A83C1: return None
    names = []; o = no
    for _ in range(nc):
        if ver >= 64:
            ln, o = ci(b, o); s = b[o:o+ln-1].decode('latin1'); o += ln
        else:
            e = b.index(b'\0', o); s = b[o:e].decode('latin1'); o = e+1
        o += 4; names.append(s)
    imports = []; o = io
    for _ in range(ic):
        cp, o = ci(b, o); cn, o = ci(b, o); pk = struct.unpack_from('<i', b, o)[0]; o += 4; on, o = ci(b, o)
        imports.append((names[cp], names[cn], pk, names[on]))
    pkgs = sorted({on for (cp,cn,pk,on) in imports if cn == 'Package' and pk == 0})
    return ver, lic, nc, ec, ic, pkgs, names
if __name__ == '__main__':
    for p in sys.argv[1:]:
        r = parse(p)
        if r is None: print(f"{os.path.basename(p)}: not a package"); continue
        ver, lic, nc, ec, ic, pkgs, names = r
        print(f"{os.path.basename(p)} size={os.path.getsize(p)} ver={ver}/{lic} names={nc} exports={ec} imports={ic} deps={' '.join(pkgs)}")
