# compare.py <EngineClasses.h> <Core.u> <Engine.u>... -- check EngineClasses.h against the shipped packages: member
# names and order, per-property offsets and class sizes under the gcc and msvc models. CMPFLAGS=-v prints every class.
import sys,os
sys.path.insert(0,os.path.dirname(os.path.abspath(sys.argv[0])))
import layout; from hdr import parse_header; from model import place
HDR=sys.argv[1]; layout.load(sys.argv[2:])
hc,ho=parse_header(HDR)
def ssize(ty): return layout.props_size(layout.byname[ty[1:]])
memo={}
def cpp(name,model):
    if (name,model) in memo: return memo[(name,model)]
    c=hc.get(name)
    if c is None:
        s=layout.props_size(layout.byname[name[1:]]); memo[(name,model)]=(s,s,{}); return memo[(name,model)]
    bd,bs,_=cpp(c['sup'],model)
    d,offs=place(c['mem'], bd if model=='gcc' else bs, model, ssize)
    memo[(name,model)]=(d,(d+3)//4*4,offs); return memo[(name,model)]
bad=0
for n in ho:
    e=layout.byname.get(n[1:])
    if e is None: print(n,'NO SCRIPT CLASS'); bad+=1; continue
    ss=layout.props_size(e); lay=layout.LAYOUT[(e['pkg'].name,e['idx'])]
    names=[l[0] for l in lay]; hn=[m['name'] for m in hc[n]['mem']]
    _,ms,mo=cpp(n,'msvc'); _,gs,go=cpp(n,'gcc')
    mism=[(l[0],l[2],go.get(l[0]),mo.get(l[0])) for l in lay if go.get(l[0])!=l[2] or mo.get(l[0])!=l[2]]
    flag = (names!=hn) or mism or ms!=ss or gs!=ss
    bad+=bool(flag)
    if flag or '-v' in os.environ.get('CMPFLAGS',''):
        print(f"{n}: script={ss} msvc={ms} gcc={gs} order_ok={names==hn} mismatches(name,script,gcc,msvc)={mism}")
print(f"native classes in header: {len(ho)}; with any issue: {bad}")
