# xadec.py <file.uax>... -- EA-XA v2 reference decoder; for up to 5 XA sounds with lipsync data (flags & 128) per
# file, correlates the stored lipsync bytes with the decoded PCM's peak per 20 ms. Imports a `uax` module (parse,
# sounds, ci) from outside this repo.
import sys,struct,glob
sys.path.insert(0,'/tmp/claude-1000/-home-ion-hp2-ufrontend/12487dd3-9ca1-402a-8034-cf740183f095/scratchpad')
from uax import *
C1=[0,240,460,392]; C2=[0,0,-208,-220]
def dec_v2(d, ns):
    out=[]; h1=h2=0; p=0
    while p<len(d) and len(out)<ns:
        info=d[p]
        if info==0xEE:
            h1=struct.unpack_from('>h',d,p+1)[0]; h2=struct.unpack_from('>h',d,p+3)[0]
            for i in range(28): out.append(struct.unpack_from('>h',d,p+5+2*i)[0])
            p+=61; continue
        c1=C1[info>>4]; c2=C2[info>>4]; sh=(info&15)+8
        for i in range(28):
            byte=d[p+1+i//2]; nib=(byte>>4)&15 if i%2==0 else byte&15
            v=((nib<<28)&0xffffffff)
            if v&0x80000000: v-=1<<32
            v=v>>sh
            s=(v + c1*h1 + c2*h2 + 128)>>8
            s=max(-32768,min(32767,s))
            out.append(s); h2=h1; h1=s
        p+=15
    return out[:ns]
def lipsync_check(fn, maxn=5):
    b,ver,names,exps=parse(fn)
    v,ss=sounds(fn)
    res=[]
    for s in ss:
        if s['filetype']!='XA' or not (s['flags']&128): continue
        d=b[s['datapos']:s['datapos']+s['cnt']]
        p=s['datapos']+s['cnt']
        seek=struct.unpack_from('<i',b,p)[0]; p+=4
        cnt,p=ci(b,p); lips=b[p:p+cnt]
        pcm=dec_v2(d,s['ns'])
        sr=s['sr']; N=len(lips)-1
        pred=[]
        for i in range(N):
            t=i*0.020
            a=int(sr*2*t)&~1; e=int(sr*2*(t+0.005))&~1
            a//=2; e//=2
            m=0
            for k in range(a,min(e,len(pcm))):
                amp=min(255,abs(pcm[k])//128)
                m=max(m,amp)
            pred.append(m)
        # correlation
        import math
        L=list(lips[:N])
        mx=sum(L)/N; my=sum(pred)/N
        num=sum((x-mx)*(y-my) for x,y in zip(L,pred))
        den=math.sqrt(sum((x-mx)**2 for x in L)*sum((y-my)**2 for y in pred)) or 1
        mad=sum(abs(x-y) for x,y in zip(L,pred))/N
        res.append((s['name'],s['ns'],N,round(num/den,4),round(mad,2),max(L),max(pred), round(max(abs(x) for x in pcm)/32768,3)))
        if len(res)>=maxn: break
    return res
for fn in sys.argv[1:]:
    for r in lipsync_check(fn): print(fn.split('/')[-1], r)
