# forscope.py FILE... -- list for-init variables (for(TYPE v=...)) used after their loop in the same block:
# code that relies on MSVC6 for-scope.
import re,sys,glob
TYPES=r'(?:INT|int|DWORD|UBOOL|BYTE|_WORD|WORD|FLOAT|float|unsigned(?:\s+int)?|long|SWORD|SBYTE|TCHAR\s*\*|UObject\s*\*|AActor\s*\*|\w+\s*\*)'
decl_re=re.compile(r'\bfor\s*\(\s*'+TYPES+r'\s+(\w+)\s*=')
def strip(line):
    line=re.sub(r'//.*','',line)
    line=re.sub(r'"(\\.|[^"\\])*"','""',line)
    line=re.sub(r"'(\\.|[^'\\])*'","''",line)
    return line
res=[]
for f in sys.argv[1:]:
    try: L=open(f,encoding='latin-1').read().split('\n')
    except: continue
    # blank out block comments, keeping line numbers
    txt='\n'.join(L)
    txt=re.sub(r'/\*.*?\*/',lambda m:'\n'*m.group(0).count('\n'),txt,flags=re.S)
    L=[strip(x) for x in txt.split('\n')]
    depth=[0]*len(L); d=0
    for i,l in enumerate(L):
        depth[i]=d
        d+=l.count('{')-l.count('}')
    for i,l in enumerate(L):
        for m in decl_re.finditer(l):
            v=m.group(1); d0=depth[i]
            # end = the loop's last line: a braced body ends when the depth returns to d0, a single statement at its ';'
            j=i; started=False
            k=i
            s=''.join(L[i:i+3])
            braced=False
            rest=l[m.start():]
            j=i+1
            end=None
            if '{' in l[m.start():] or (i+1<len(L) and L[i+1].strip().startswith('{')):
                for j in range(i+1,min(len(L),i+2000)):
                    if depth[j]==d0 and j>i+ (0 if '{' in l[m.start():] else 1) and depth[j-1]>d0:
                        end=j-1;break
                    if depth[j]<d0: end=j-1;break
            else:
                # single statement: ends at first line (from i) ending with ';' after for-paren closes
                for j in range(i,min(len(L),i+20)):
                    if L[j].rstrip().endswith(';') and not (j==i and L[j].count(';')<=2 and L[j].rstrip().endswith(')')):
                        if j==i and L[j].count(';')<3: continue
                        end=j;break
            if end is None: continue
            # first use of v after the loop, within the enclosing block (a redeclaration ends the search)
            for j in range(end+1,min(len(L),end+400)):
                if depth[j]<d0: break
                lj=L[j]
                if re.search(r'\b'+TYPES+r'\s+'+v+r'\b',lj) : break
                if re.search(r'\bfor\s*\(\s*'+v+r'\s*=',lj) and False: pass
                if re.search(r'(?<![\.\>:\w])'+v+r'\b',lj):
                    res.append((f,i+1,v,j+1,lj.strip()[:100]))
                    break
for r in res: print('%s:%d var %s used at %d: %s'%r)
