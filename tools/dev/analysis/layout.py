# Emulates HP2 433's UStruct::Link over the .u packages named on the command line: per-property offsets (LAYOUT)
# and PropertiesSize (SIZE). compare.py runs it in its own namespace.
import sys,struct,re
sys.path.insert(0,sys.argv[0].rsplit('/',1)[0])
from upkg import *
PROPS={'ByteProperty','IntProperty','BoolProperty','FloatProperty','ObjectProperty','ClassProperty','NameProperty','StrProperty','StringProperty','FixedArrayProperty','ArrayProperty','MapProperty','StructProperty'}
STRUCTY={'Class','Struct','State','Function'}
pkgs=[Pkg(p) for p in sys.argv[1:]]
# global object table keyed by (pkgname, exportindex)
objs={}
byname={}
for pk in pkgs:
    for e in pk.exports:
        e['pkg']=pk; e['cls']=pk.clsname(e)
        objs[(pk.name,e['idx'])]=e
        if e['cls'] in ('Class','Struct'):
            byname.setdefault(e['name'],e)
def resolve(pk,r):
    """object ref -> export dict (following imports by name into loaded packages)"""
    if r==0: return None
    if r>0: return pk.exports[r-1]
    im=pk.imports[-r-1]
    return byname.get(im['name']) if im['cls'] in ('Class','Struct') else None
def parse_field(e):
    if 'parsed' in e: return
    pk=e['pkg']; c=e['cls']
    sup,nxt,p=pk.fieldhdr(e, c=='Class')
    e['next']=nxt; e['superfield']=sup
    if c in STRUCTY:
        st,p=pk.ci(p); ch,p=pk.ci(p); e['children']=ch
    if c in PROPS:
        dim,fl=struct.unpack_from('<iI',pk.d,p); p+=8; cat,p=pk.ci(p)
        if fl&CPF_Net: p+=2
        e['dim']=dim; e['pflags']=fl
        if c=='ByteProperty': e['enum'],p=pk.ci(p)
        elif c in('ObjectProperty',): e['pclass'],p=pk.ci(p)
        elif c=='ClassProperty': e['pclass'],p=pk.ci(p); e['meta'],p=pk.ci(p)
        elif c=='StructProperty': e['struct'],p=pk.ci(p)
        elif c=='FixedArrayProperty': e['inner'],p=pk.ci(p); e['count']=struct.unpack_from('<i',pk.d,p)[0]; p+=4
        elif c=='ArrayProperty': e['inner'],p=pk.ci(p)
        elif c=='MapProperty': e['key'],p=pk.ci(p); e['value'],p=pk.ci(p)
        elif c=='StringProperty': e['strsize']=struct.unpack_from('<i',pk.d,p)[0]; p+=4
    e['parsed']=1
SIZE={}
def elem(e):
    """(ElementSize, align) per HP2 433 Link rules"""
    c=e['cls']
    if c=='ByteProperty': return 1,1
    if c in('IntProperty','FloatProperty','ObjectProperty','ClassProperty','NameProperty'): return 4,4
    if c=='BoolProperty': return 4,4
    if c in('StrProperty','ArrayProperty'): return 12,4
    if c=='MapProperty': return 20,4
    if c=='StringProperty': return e['strsize'],4
    if c=='StructProperty':
        st=resolve(e['pkg'],e['struct']); sz=props_size(st)
        return sz,(2 if sz==2 else 4 if sz>=4 else 1)
    if c=='FixedArrayProperty':
        inn=resolve(e['pkg'],e['inner']) if e['inner']>0 else None
        parse_field(inn); es,_=elem(inn); return es*e['count'],4
    raise Exception('unknown prop '+c)
def align(x,a): return (x+a-1)//a*a
LAYOUT={}
def props_size(st):
    key=(st['pkg'].name,st['idx'])
    if key in SIZE: return SIZE[key]
    parse_field(st)
    size=0
    if st['superref']:
        sup=resolve(st['pkg'],st['superref'])
        if sup is None: raise Exception('unresolved super of '+st['name'])
        size=props_size(sup)
    lay=[]; prevbool=None; bit=0
    r=st['children']
    while r:
        f=st['pkg'].exports[r-1] if r>0 else None
        if f is None: break
        if f['outer']!=st['idx']+1: break
        parse_field(f)
        if f['cls'] in PROPS:
            es,al=elem(f)
            if f['cls']=='BoolProperty' and prevbool is not None and st['cls']!='Function' and bit<31:
                off=prevbool['offset']; bit+=1
            else:
                off=align(size,al); bit=0
            f['offset']=off; f['esize']=es
            size=off+es*f['dim']
            prevbool=f if f['cls']=='BoolProperty' else None
            lay.append((f['name'],f['cls'],off,es*f['dim'],f['dim'],bit if f['cls']=='BoolProperty' else None))
        r=f['next']
    size=align(size,4)
    SIZE[key]=size; LAYOUT[key]=lay
    return size
