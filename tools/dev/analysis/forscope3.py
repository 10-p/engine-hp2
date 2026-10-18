# forscope3.py <forscope.py> FILE... -- run forscope.py on the files and drop a hit if the name is declared (outside
# a for-init) between the enclosing function's opening '{' (column 0) and the use line.
import re,sys,subprocess
TY=r'(?:INT|int|DWORD|UBOOL|BYTE|_WORD|WORD|FLOAT|float|unsigned|long|SWORD|SBYTE|TCHAR|[A-Z]\w*)'
S=sys.argv[1]
out=subprocess.run(['python3',S]+sys.argv[2:],capture_output=True,text=True).stdout
cache={}
for line in out.splitlines():
    m=re.match(r'(.*?):(\d+) var (\w+) used at (\d+):',line)
    if not m: continue
    f,d,nm,u=m.group(1),int(m.group(2)),m.group(3),int(m.group(4))
    if f not in cache: cache[f]=open(f,encoding='latin-1').read().split('\n')
    L=cache[f]
    s=d-1
    while s>0 and not L[s].startswith('{'): s-=1
    decl=re.compile(r'(?<!for\s)(?<!for)\b'+TY+r'\s*\*?\s*(?:\w+\s*(?:=[^,;]*)?,\s*)*\b'+nm+r'\b\s*(?:[=;,\[(])')
    fixed=False
    for k in range(s,u-1):
        t=re.sub(r'//.*','',L[k])
        if re.search(r'\bfor\s*\(',t): continue
        if decl.search(t): fixed=True;break
    if not fixed: print(line)
