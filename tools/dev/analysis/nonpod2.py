# nonpod2.py FILE... (run from Source/) -- print printf-style call arguments that are FString variables/members
# or FString-valued expressions passed by value through varargs.
import re,sys,glob
# collect FString-typed member/variable names across the files and Core/Inc, Engine/Inc headers
names=set()
files=sys.argv[1:]
for f in files+glob.glob('Core/Inc/*.h')+glob.glob('Engine/Inc/*.h'):
    t=open(f,encoding='latin-1').read()
    for m in re.finditer(r'\bFString(?:NoInit)?\s+([A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*)\s*(?:;|=|\[)',t):
        for n in m.group(1).split(','): names.add(n.strip())
names-= {'Printf','operator'}
VAR=r'\b(?:debugf|debugfSlow|appErrorf|appThrowf|appUnwindf|appMsgf|Logf|appSprintf|Printf|warnf|StatusUpdatef|YesNof|appFailAssert)\s*\('
for f in files:
    t=open(f,encoding='latin-1').read()
    t2=re.sub(r'/\*.*?\*/',lambda m:' '*len(m.group(0)) if '\n' not in m.group(0) else '\n'*m.group(0).count('\n'),t,flags=re.S)
    t2=re.sub(r'//[^\n]*','',t2)
    for m in re.finditer(VAR,t2):
        i=m.end(); lvl=1; cur=''; args=[]; 
        while i<len(t2) and lvl>0:
            c=t2[i]
            if c=='"':
                j=i+1
                while j<len(t2) and t2[j]!='"':
                    if t2[j]=='\\': j+=1
                    j+=1
                cur+=t2[i:j+1]; i=j+1; continue
            if c in '([{': lvl+=1
            elif c in ')]}':
                lvl-=1
                if lvl==0: args.append(cur); break
            elif c==',' and lvl==1: args.append(cur); cur=''; i+=1; continue
            cur+=c; i+=1
        fi=None
        for k,a in enumerate(args):
            if 'TEXT(' in a or a.strip().startswith('"') or 'Localize' in a: fi=k;break
        if fi is None: continue
        for a in args[fi+1:]:
            s=a.strip()
            if s.startswith('*') or s.startswith('&'): continue
            last=re.split(r'->|\.',s)[-1].strip()
            if re.fullmatch(r'\w+',last) and last in names and not re.search(r'\(\s*$',s):
                print('%s:%d: %s'%(f,t2[:m.start()].count('\n')+1,s[:70]))
            elif re.search(r'\.(Left|Right|Mid|Caps|Locs|LeftChop)\([^)]*\)\s*$',s) or re.search(r'FString\s*\(',s) or re.search(r'\bGetPathName\(\)|String\(\)\s*$',s):
                print('%s:%d: EXPR %s'%(f,t2[:m.start()].count('\n')+1,s[:70]))
