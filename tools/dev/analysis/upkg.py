# Minimal UE1 package reader: names, imports, exports and an export's tagged property list.
import struct, sys
class R:
    def __init__(s,d,p=0): s.d=d; s.p=p
    def u8(s): v=s.d[s.p]; s.p+=1; return v
    def i32(s): v=struct.unpack_from('<i',s.d,s.p)[0]; s.p+=4; return v
    def u32(s): v=struct.unpack_from('<I',s.d,s.p)[0]; s.p+=4; return v
    def u16(s): v=struct.unpack_from('<H',s.d,s.p)[0]; s.p+=2; return v
    def ci(s):
        b0=s.u8(); neg=b0&0x80; v=b0&0x3f
        if b0&0x40:
            b1=s.u8(); v|=(b1&0x7f)<<6
            if b1&0x80:
                b2=s.u8(); v|=(b2&0x7f)<<13
                if b2&0x80:
                    b3=s.u8(); v|=(b3&0x7f)<<20
                    if b3&0x80:
                        b4=s.u8(); v|=b4<<27
        return -v if neg else v
class Pkg:
    def __init__(s,path):
        d=open(path,'rb').read(); s.d=d; r=R(d)
        tag=r.u32(); ver=r.u16(); lic=r.u16(); flags=r.u32()
        nc,no,ec,eo,ic,io=[r.u32() for _ in range(6)]
        s.ver=ver
        r.p=no; s.names=[]
        for i in range(nc):
            if ver>=64:
                l=r.ci(); nm=d[r.p:r.p+l-1].decode('latin1'); r.p+=l
            else:
                e=d.index(b'\0',r.p); nm=d[r.p:e].decode('latin1'); r.p=e+1
            r.u32(); s.names.append(nm)
        r.p=io; s.imports=[]
        for i in range(ic):
            cp=r.ci(); cn=r.ci(); pk=r.i32(); on=r.ci(); s.imports.append((s.names[cn],s.names[on]))
        r.p=eo; s.exports=[]
        for i in range(ec):
            cl=r.ci(); su=r.ci(); pk=r.i32(); on=r.ci(); fl=r.u32(); sz=r.ci(); off=r.ci() if sz>0 else 0
            s.exports.append(dict(cls=cl,name=s.names[on],size=sz,off=off,pkg=pk))
    def clsname(s,ci):
        if ci<0: return s.imports[-ci-1][1]
        if ci>0: return s.exports[ci-1]['name']
        return 'Class'
    def props(s,e):
        r=R(s.d,e['off']); out={}
        while True:
            ni=r.ci(); nm=s.names[ni]
            if nm=='None': break
            info=r.u8(); typ=info&0xf; szc=(info>>4)&7; arr=info&0x80
            if typ==10: sn=r.ci()
            size={0:1,1:2,2:4,3:12,4:16}.get(szc)
            if szc==5: size=r.u8()
            elif szc==6: size=r.u16()
            elif szc==7: size=r.i32()
            idx=0
            if arr and typ!=3:
                b=r.u8()
                if b&0x80: r.p-=1; idx=r.u16()&0x3fff if not (b&0x40) else (r.u32()&0x3fffffff)
                else: idx=b
            start=r.p
            if typ==3: val=bool(arr)
            elif typ==5: val=r.ci(); r.p=start+size
            else: val=s.d[start:start+size]; r.p=start+size
            out[(nm,idx)]=(typ,val)
        return out,r.p
