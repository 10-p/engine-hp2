#!/usr/bin/env python3
"""Minimal UE1 package parser: dumps USound exports of a .uax (FileType, CoreFlags, raw_*, data magic)."""
import struct, sys

def rd_index(b, p):
    # UE1 compact index
    v = b[p]; p += 1
    neg = v & 0x80
    n = v & 0x3f
    if v & 0x40:
        shift = 6
        while True:
            v = b[p]; p += 1
            n |= (v & 0x7f) << shift
            shift += 7
            if not (v & 0x80): break
    return (-n if neg else n), p

def rd_str(b, p):
    n, p = rd_index(b, p)
    s = b[p:p+n-1].decode('latin1', 'replace') if n > 0 else ''
    return s, p + n

def main(path, limit=None):
    b = open(path, 'rb').read()
    tag, ver = struct.unpack_from('<IH', b, 0)
    licver, = struct.unpack_from('<H', b, 6)
    flags, nname, oname, nexp, oexp, nimp, oimp = struct.unpack_from('<IiIiIiI', b, 8)
    print(f"tag={tag:08x} ver={ver} lic={licver} flags={flags:x} names={nname} exports={nexp} imports={nimp}")
    names = []
    p = oname
    for i in range(nname):
        s, p = rd_str(b, p)
        p += 4  # flags
        names.append(s)
    imports = []
    p = oimp
    for i in range(nimp):
        cp, p = rd_index(b, p); cn, p = rd_index(b, p)
        pk = struct.unpack_from('<i', b, p)[0]; p += 4
        on, p = rd_index(b, p)
        imports.append((names[cp], names[cn], pk, names[on]))
    exports = []
    p = oexp
    for i in range(nexp):
        cls, p = rd_index(b, p); sup, p = rd_index(b, p)
        pk = struct.unpack_from('<i', b, p)[0]; p += 4
        on, p = rd_index(b, p)
        of = struct.unpack_from('<I', b, p)[0]; p += 4
        sz, p = rd_index(b, p)
        off = 0
        if sz > 0:
            off, p = rd_index(b, p)
        exports.append((cls, sup, pk, names[on], of, sz, off))
    def objname(idx):
        if idx > 0: return exports[idx-1][3]
        if idx < 0: return imports[-idx-1][3]
        return 'None'
    cnt = {}
    shown = 0
    for (cls, sup, pk, nm, of, sz, off) in exports:
        cname = objname(cls)
        if cname != 'Sound': continue
        q = off
        # UObject::Serialize for a non-actor: property list terminated by 'None' name index (compact index)
        ni, q = rd_index(b, q)
        if names[ni] != 'None':
            print(f"  {nm}: unexpected first property {names[ni]!r}; skipping")
            continue
        ft, q = rd_index(b, q)
        coreflags, dur = struct.unpack_from('<if', b, q); q += 8
        rs = rbps = rch = rsr = None
        if ver > 76: rs, = struct.unpack_from('<i', b, q); q += 4
        if ver > 77: rbps, rch = struct.unpack_from('<ii', b, q); q += 8
        if ver > 78: rsr, = struct.unpack_from('<i', b, q); q += 4
        savedpos, = struct.unpack_from('<i', b, q); q += 4   # TLazyArray: SavedPos (INT) then count
        n, q = rd_index(b, q)
        magic = b[q:q+16]
        key = (names[ft], coreflags)
        cnt[key] = cnt.get(key, 0) + 1
        if limit is None or shown < limit:
            print(f"  {nm:32s} FileType={names[ft]:4s} CoreFlags=0x{coreflags:03x} Dur={dur:7.3f} raw(ns={rs},bps={rbps},ch={rch},sr={rsr}) DataBytes={n:8d} magic={magic[:12].hex()} {magic[:4]!r}")
            shown += 1
    print("summary (FileType, CoreFlags) -> count:")
    for k, v in sorted(cnt.items()): print("  ", k, v)

if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else None)
