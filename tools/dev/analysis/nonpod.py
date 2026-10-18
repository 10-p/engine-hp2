# nonpod.py FILE... -- heuristic: print printf-style calls (debugf, appErrorf, Printf, ...) whose variadic
# arguments include an FString by value, which clang compiles into a runtime trap.
import re,sys
VAR=r'(?:debugf|debugfSlow|appErrorf|appThrowf|appUnwindf|appMsgf|Logf|appSprintf|Printf|appFailAssert|warnf|StatusUpdatef|YesNof|WrappedPrintf|appSnprintf|FString::Printf)'
call_re=re.compile(VAR+r'\s*\(')
def args_of(s,start):
    # s[start] == '(' ; return list of top-level args and end index
    lvl=0; cur=''; out=[]; i=start
    while i<len(s):
        c=s[i]
        if c=='"':
            j=i+1
            while j<len(s) and s[j]!='"':
                if s[j]=='\\': j+=1
                j+=1
            cur+=s[i:j+1]; i=j+1; continue
        if c in '([{':
            lvl+=1
            if lvl==1: i+=1; continue
        elif c in ')]}':
            lvl-=1
            if lvl==0: out.append(cur.strip()); return out,i
        elif c==',' and lvl==1:
            out.append(cur.strip()); cur=''; i+=1; continue
        cur+=c; i+=1
    return out,i
for f in sys.argv[1:]:
    txt=open(f,encoding='latin-1').read()
    txt=re.sub(r'/\*.*?\*/',' ',txt,flags=re.S); txt=re.sub(r'//[^\n]*','',txt)
    fvars=set(re.findall(r'\bFString\s+(\w+)\s*[=;(,)]',txt))
    fvars|=set(re.findall(r'\bFString\s*&\s*(\w+)',txt))
    fvars|=set(re.findall(r'\bFStringNoInit\s+(\w+)',txt))
    fvars-= {'Printf','operator'}
    for m in call_re.finditer(txt):
        a,e=args_of(txt,m.end()-1)
        if len(a)<2: continue
        # find the format arg index = first arg containing TEXT( or a string literal
        fi=None
        for k,x in enumerate(a):
            if 'TEXT(' in x or x.startswith('"') or 'Localize' in x: fi=k;break
        if fi is None: continue
        for x in a[fi+1:]:
            xs=x.strip()
            if xs in fvars or re.fullmatch(r'\w+\.(Map|Protocol|Host|Portal|Error|Text|String|Name)',xs) or re.search(r'FString\s*\(',xs) and not xs.startswith('*') or re.fullmatch(r'\w+\.(Left|Right|Mid|Caps|Locs)\(.*\)',xs) or xs.endswith('GetPathNameSafe()') :
                line=txt[:m.start()].count('\n')+1
                print('%s: ~%d: arg "%s" in %s'%(f,line,xs[:60],txt[m.start():m.start()+60].replace('\n',' ')))
