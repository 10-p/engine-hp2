# template-engine analyst: emit compile-time layout asserts (script offsets from the shipped .u)
import sys,os
sys.path.insert(0,os.path.dirname(os.path.abspath(sys.argv[0])))
import layout; from hdr import parse_header
HDR=sys.argv[1]; layout.load(sys.argv[2:])
hc,ho=parse_header(HDR)
out=['// GENERATED from Assets/System/{Core,Engine}.u by emit_asserts.py -- do not edit.',
     '// Every non-bitfield member of every native class in EngineClasses.h must sit at the offset',
     '// the shipped Engine.u gives its property; every class must have the script PropertiesSize.',
     '#include "EnginePrivate.h"','#define UE_OFS(C,M) __builtin_offsetof(C,M)','']
n=0
for c in ho:
    e=layout.byname[c[1:]]; ss=layout.props_size(e); L=layout.LAYOUT[(e['pkg'].name,e['idx'])]
    bits={m['name'] for m in hc[c]['mem'] if m['bits']}
    for name,cls,off,sz,dim in L:
        if name in bits: continue
        out.append(f'static_assert(UE_OFS({c},{name})=={off}, "{c}::{name} must be at script offset {off}");'); n+=1
    out.append(f'static_assert(sizeof({c})=={ss}, "sizeof({c}) must equal script PropertiesSize {ss}");'); n+=1
open('EngineLayoutAsserts.cpp','w').write('\n'.join(out)+'\n')
print(n,'asserts')
