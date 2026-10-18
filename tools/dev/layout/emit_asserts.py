# emit_asserts.py <EngineClasses.h> <Core.u> <Engine.u>... -- write EngineLayoutAsserts.cpp into the current directory:
# a static_assert per non-bitfield member offset and per class size, taken from the shipped packages.
import sys,os
sys.path.insert(0,os.path.dirname(os.path.abspath(sys.argv[0])))
import layout; from hdr import parse_header
HDR=sys.argv[1]; layout.load(sys.argv[2:])
hc,ho=parse_header(HDR)
out=['// GENERATED from Assets/System/{Core,Engine}.u by tools/dev/layout/emit_asserts.py -- do not edit by hand.',
     '//',
     '// Compile-time layout check: EngineClasses.h is frozen data that must match the shipped, never-recompiled',
     '// Engine.u byte for byte. Every non-bitfield member of every native class in it must sit at the offset the',
     '// loaded .u gives its property, and every class must have the script PropertiesSize. The runtime [layout]',
     '// size guard (Core UnClass.cpp) is necessary but not sufficient: a derived class\'s members can move into',
     '// the base\'s tail padding with both sizes unchanged (ABrush/AMover). These asserts see that. Offsets are',
     '// target-independent (i386 SysV, wasm32 with -fpack-struct=4, and MinGW\'s -mms-bitfields all reach the',
     '// script layout -- the latter without the bColoredPad/bTwoWayPad pads), so a failure here is a real layout',
     '// defect: fix the header, never this file. The offsets are the 32-bit script layout, so a 64-bit build skips them',
     '// (there UStruct::Link\'s size guard and tools/dev/layout/layout-sweep.gdb check the layout).',
     '//',
     '// Regenerate after any change to EngineClasses.h or the shipped .u, from Source/Engine/Src:',
     '//   python3 ../../../tools/dev/layout/emit_asserts.py ../Inc/EngineClasses.h <assets>/System/Core.u <assets>/System/Engine.u',
     '// offsetof on a non-standard-layout class (they all have vtables) is conditionally supported; GCC and clang',
     '// evaluate __builtin_offsetof as a constant here and only warn, hence the pragma.',
     '#include "EnginePrivate.h"','',
     '#if __GNUC__','#pragma GCC diagnostic ignored "-Winvalid-offsetof"','#endif','',
     '#define UE_OFS(C,M) __builtin_offsetof(C,M)','',
     '#if !PLATFORM_64BITS','']
n=0
for c in ho:
    e=layout.byname[c[1:]]; ss=layout.props_size(e); L=layout.LAYOUT[(e['pkg'].name,e['idx'])]
    bits={m['name'] for m in hc[c]['mem'] if m['bits']}
    for name,cls,off,sz,dim in L:
        if name in bits: continue
        out.append(f'static_assert(UE_OFS({c},{name})=={off}, "{c}::{name} must be at script offset {off}");'); n+=1
    out.append(f'static_assert(sizeof({c})=={ss}, "sizeof({c}) must equal script PropertiesSize {ss}");'); n+=1
out+=['','#endif // !PLATFORM_64BITS']
open('EngineLayoutAsserts.cpp','w').write('\n'.join(out)+'\n')
print(n,'asserts')
