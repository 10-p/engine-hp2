# undc.py <file> > out -- print the file with #ifdef/#if defined PLATFORM_DREAMCAST / PLATFORM_LOW_MEMORY blocks
# resolved as false (their else branch kept).
import sys,re
lines=open(sys.argv[1],encoding='latin-1').read().replace('\r','').split('\n')
out=[];stack=[]  # each: [kind ('dc'|'other'), skipping]
for l in lines:
    s=l.strip()
    m=re.match(r'#\s*(ifdef|ifndef|if)\b(.*)',s)
    if m:
        cond=m.group(2)
        dc=('PLATFORM_DREAMCAST' in cond or 'PLATFORM_LOW_MEMORY' in cond) and '&&' not in cond and '||' not in cond
        if dc:
            neg = m.group(1)=='ifndef' or '!' in cond
            stack.append(['dc', not neg]);   # skipping true branch if not neg
            continue
        stack.append(['other',False]); 
        if all(not x[1] for x in stack if x[0]=='dc'): out.append(l)
        continue
    if re.match(r'#\s*else',s) and stack:
        if stack[-1][0]=='dc': stack[-1][1]=not stack[-1][1]; continue
    if re.match(r'#\s*endif',s) and stack:
        t=stack.pop()
        if t[0]=='dc': continue
    if all(not x[1] for x in stack if x[0]=='dc'): out.append(l)
print('\n'.join(out))
