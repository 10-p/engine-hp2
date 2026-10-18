# Minimal UE1 package reader (package versions 61-79): name, import and export tables, field headers.
import struct,sys,os
RF_HasStack=0x02000000
CPF_Net=0x20
class Pkg:
    def __init__(s,path):
        s.path=path; s.name=os.path.splitext(os.path.basename(path))[0]
        d=s.d=open(path,'rb').read()
        tag,ver,flags,nc,no,ec,eo,ic,io=struct.unpack_from('<9I',d,0)
        assert tag==0x9E2A83C1
        s.ver=ver&0xffff; s.lic=ver>>16
        s.names=[]; p=no
        for i in range(nc):
            if s.ver>=64:
                n,p=s.ci(p); st=d[p:p+n-1].decode('latin-1'); p+=n; p+=4
            else:
                e=d.index(b'\0',p); st=d[p:e].decode('latin-1'); p=e+1+4
            s.names.append(st)
        s.imports=[]; p=io
        for i in range(ic):
            cp,p=s.ci(p); cn,p=s.ci(p); pk=struct.unpack_from('<i',d,p)[0]; p+=4; on,p=s.ci(p)
            s.imports.append(dict(cpkg=s.names[cp],cls=s.names[cn],outer=pk,name=s.names[on]))
        s.exports=[]; p=eo
        for i in range(ec):
            c,p=s.ci(p); su,p=s.ci(p); pk=struct.unpack_from('<i',d,p)[0]; p+=4; on,p=s.ci(p)
            fl=struct.unpack_from('<I',d,p)[0]; p+=4; sz,p=s.ci(p); off=0
            if sz: off,p=s.ci(p)
            s.exports.append(dict(clsref=c,superref=su,outer=pk,name=s.names[on],flags=fl,size=sz,off=off,idx=i))
    def ci(s,p):
        d=s.d; b=d[p]; p+=1; neg=b&0x80; v=b&0x3f
        if b&0x40:
            sh=6
            while True:
                b=d[p]; p+=1; v|=(b&0x7f)<<sh; sh+=7
                if not b&0x80: break
        return (-v if neg else v),p
    def refname(s,r):
        if r==0: return None
        if r>0: return s.exports[r-1]['name']
        return s.imports[-r-1]['name']
    def clsname(s,e):
        return s.refname(e['clsref']) or 'Class'
    def fieldhdr(s,e,isclass):
        p=e['off']
        if e['flags']&RF_HasStack:
            node,p=s.ci(p); sn,p=s.ci(p); p+=8; p+=4
            if node: _,p=s.ci(p)
        if not isclass:
            t,p=s.ci(p)
            assert s.names[t]=='None',(e['name'],s.names[t])
        sup,p=s.ci(p); nxt,p=s.ci(p)
        return sup,nxt,p
