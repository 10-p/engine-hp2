import sys,re,math
SP=sys.argv[1]; HDR=sys.argv[2]; PKGS=sys.argv[3:]
sys.argv=['x']+PKGS
exec(open(SP+'/layout.py').read())
exec(open(SP+'/hdr.py').read())
hc,ho=parse_header(HDR)
def sname(cpp): return cpp[1:]
BASIC={'BYTE':(1,1),'INT':(4,4),'FLOAT':(4,4),'DWORD':(4,4),'FName':(4,4),'FStringNoInit':(12,4),'FString':(12,4),'UBOOL':(4,4)}
def structsize(ty):
    n=ty[1:] if ty.startswith('F') else ty
    e=byname.get(n)
    if e is None: return None
    return props_size(e)
def msize(m):
    ty=m['type']
    if ty.endswith('*'): return 4,4
    if ty.startswith('TArray'): return 12,4
    if ty in BASIC: return BASIC[ty]
    s=structsize(ty)
    if s is None: raise Exception('unknown type '+ty)
    return s,(4 if s>=4 else s)
def dimval(d):
    if d is None: return 1
    if d.isdigit(): return int(d)
    raise Exception('symbolic dim '+d)
def cpp_layout(name,model,memo):
    """model: 'gcc' (SysV/wasm: packed lone bitfields, tail-padding reuse) or 'msvc' (retail /Zp4: DWORD bitfield units, no tail reuse).
    returns (dsize, size, {member:offset})"""
    key=(name,model)
    if key in memo: return memo[key]
    c=hc.get(name)
    if c is None:
        # super outside EngineClasses.h: trust script size (UObject=40 etc.)
        e=byname.get(sname(name)); s=props_size(e)
        memo[key]=(s,s,{}); return memo[key]
    bd,bs,_=cpp_layout(c['sup'],model,memo)
    off=bd if model=='gcc' else bs
    offs={}; bitpos=None  # bitpos: (unitstart_byte, bits_used)
    for m in c['mem']:
        if m['bits']:
            if bitpos is None or m['pack'] or bitpos[1]>=32:
                start=(off+3)//4*4
                bitpos=[start,0]
            offs[m['name']]=bitpos[0]
            bitpos[1]+=1
            off = bitpos[0] + (math.ceil(bitpos[1]/8) if model=='gcc' else 4)
            continue
        bitpos=None
        sz,al=msize(m)
        if m['pack'] or m['align']: al=max(al,4)
        off=(off+al-1)//al*al
        offs[m['name']]=off
        off+=sz*dimval(m['dim'])
    dsize=off; size=(off+3)//4*4
    memo[key]=(dsize,size,offs); return memo[key]
memo={}
tot=0; bad=0
print("class | script PropertiesSize | msvc sizeof | gcc sizeof | name-order ok | per-property mismatches (gcc model)")
for n in ho:
    e=byname.get(sname(n))
    if e is None: print(n,'NO SCRIPT CLASS'); continue
    ssz=props_size(e); lay=LAYOUT[(e['pkg'].name,e['idx'])]
    names=[l[0] for l in lay]; hn=[m['name'] for m in hc[n]['mem']]
    ok = names==hn
    _,ms,mo=cpp_layout(n,'msvc',memo); _,gs,go=cpp_layout(n,'gcc',memo)
    mism=[(l[0],l[2],go.get(l[0]),mo.get(l[0])) for l in lay if go.get(l[0])!=l[2] or mo.get(l[0])!=l[2]]
    tot+=1
    if not ok or mism or ms!=ssz or gs!=ssz: bad+=1
    print(f"{n} | {ssz} | {ms} | {gs} | {ok} | {mism[:6]}")
    if not ok:
        print('   script:',names[:8],'... hdr:',hn[:8])
print('classes',tot,'with issues',bad)
