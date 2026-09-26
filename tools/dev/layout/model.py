# template-engine analyst: C++ layout models. 'gcc' = SysV i386 / wasm32 clang (GCC_PACK'd lone bitfield
# runs occupy ceil(bits/8) bytes; derived members may use the base's tail padding). 'msvc' = Win32 /Zp4 and
# MinGW -mms-bitfields (bitfield runs occupy whole DWORDs; derived class starts at sizeof(base)).
import math
BASIC={'BYTE':(1,1),'INT':(4,4),'FLOAT':(4,4),'DWORD':(4,4),'FName':(4,4),'FStringNoInit':(12,4),'FString':(12,4),'UBOOL':(4,4),'BITFIELD':(4,4),'FColor':(4,4),'SWORD':(2,2),'_WORD':(2,2),'float':(4,4),'int':(4,4),'FTime':(8,4),'DOUBLE':(8,4),'QWORD':(8,4),'SQWORD':(8,4),'FVector':(12,4),'FRotator':(12,4),'FPlane':(16,4),'FCoords':(48,4)}
def msize(ty,structsize):
    if ty.endswith('*'): return 4,4
    if ty.startswith('TArray'): return 12,4
    if ty in BASIC: return BASIC[ty]
    s=structsize(ty)
    return s,(4 if s>=4 else s)
def place(members,start,model,structsize):
    off=start; offs={}; bp=None
    for m in members:
        if m['bits']:
            if bp is None or m['pack'] or bp[1]>=32: bp=[(off+3)//4*4,0]
            offs[m['name']]=bp[0]; bp[1]+=1
            off=bp[0]+(math.ceil(bp[1]/8) if model=='gcc' else 4); continue
        bp=None
        sz,al=msize(m['type'],structsize)
        if m['pack'] or m['align']: al=max(al,4)
        off=(off+al-1)//al*al; offs[m['name']]=off
        d=int(m['dim']) if m['dim'] else 1
        off+=sz*d
    return off,offs
