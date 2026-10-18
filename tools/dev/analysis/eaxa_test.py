#!/usr/bin/env python3
"""eaxa_test.py <file.uax> <SoundName> -- sanity-decode an HP2 'XA' sound as EA-XA ADPCM v2 (15-byte blocks,
28 samples, mono): finds the export with uaxdump's readers, prints stats and writes <SoundName>.wav beside this script."""
import sys, struct, wave, os
sys.path.insert(0, os.path.dirname(__file__))
import uaxdump as U

EA_TABLE = [0, 240, 460, 392, 0, 0, -208, -220]  # coef1[0..3], coef2[4..7] (ffmpeg ea_adpcm_table)

def clip16(v):
    return -32768 if v < -32768 else (32767 if v > 32767 else v)

def decode(blocks, nsamples):
    out = []
    p1 = p2 = 0
    p = 0
    escapes = 0
    while p + 15 <= len(blocks) and len(out) < nsamples:
        hdr = blocks[p]
        if hdr == 0xEE:
            # escape: 2 prev samples then 28 raw 16-bit samples (as in ffmpeg adpcm_ea_r1)
            escapes += 1
            p1, p2 = struct.unpack_from('>hh', blocks, p+1)
            raw = struct.unpack_from('>28h', blocks, p+5)
            out.extend(raw)
            p += 5 + 56
            continue
        c1 = EA_TABLE[hdr >> 4]
        c2 = EA_TABLE[(hdr >> 4) + 4]
        shift = (hdr & 0x0F) + 8
        for i in range(14):
            b = blocks[p+1+i]
            for nib in ((b >> 4) & 0xF, b & 0xF):
                s = (nib ^ 8) - 8  # sign-extend 4-bit
                s = (s << 28) >> shift
                s = (s + p1 * c1 + p2 * c2 + 0x80) >> 8
                s = clip16(s)
                out.append(s)
                p2, p1 = p1, s
        p += 15
    return out[:nsamples], escapes, p

def main(path, name):
    b = open(path, 'rb').read()
    tag, ver = struct.unpack_from('<IH', b, 0)
    flags, nname, oname, nexp, oexp, nimp, oimp = struct.unpack_from('<IiIiIiI', b, 8)
    names = []; p = oname
    for i in range(nname):
        s, p = U.rd_str(b, p); p += 4; names.append(s)
    p = oexp
    for i in range(nexp):
        cls, p = U.rd_index(b, p); sup, p = U.rd_index(b, p); p += 4
        on, p = U.rd_index(b, p); p += 4; sz, p = U.rd_index(b, p)
        off = 0
        if sz > 0: off, p = U.rd_index(b, p)
        if names[on] != name: continue
        q = off
        ni, q = U.rd_index(b, q)
        ft, q = U.rd_index(b, q)
        coreflags, dur = struct.unpack_from('<if', b, q); q += 8
        rs, = struct.unpack_from('<i', b, q); q += 4
        rbps, rch = struct.unpack_from('<ii', b, q); q += 8
        rsr, = struct.unpack_from('<i', b, q); q += 4
        q += 4  # SavedPos
        n, q = U.rd_index(b, q)
        data = b[q:q+n]
        print(f"{name}: FileType={names[ft]} flags=0x{coreflags:x} dur={dur:.3f} ns={rs} bps={rbps} ch={rch} sr={rsr} bytes={n} blocks={n/15:.2f} ns/28={rs/28:.2f}")
        pcm, esc, used = decode(data, rs)
        peak = max(abs(x) for x in pcm); rms = (sum(x*x for x in pcm)/len(pcm))**0.5
        # count runs at the rails (a wrong coefficient table explodes into clipping)
        rails = sum(1 for x in pcm if abs(x) >= 32767)
        print(f"  decoded {len(pcm)} samples (used {used} bytes, {esc} escape blocks): peak={peak} rms={rms:.1f} rail-hits={rails} ({100.0*rails/len(pcm):.3f}%)")
        outp = os.path.join(os.path.dirname(__file__), name + '.wav')
        with wave.open(outp, 'wb') as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(rsr)
            w.writeframes(struct.pack('<%dh' % len(pcm), *pcm))
        print("  wrote", outp)
        return
    print("not found")

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
