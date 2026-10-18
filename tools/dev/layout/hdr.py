# EngineClasses.h parser: each native class's superclass and data members (type, name, array dim, bitfield,
# GCC_PACK/GCC_ALIGN). Imported by compare.py and emit_asserts.py.
import re
MEM=re.compile(r'^(class\s+\w+\s*\*|struct\s+\w+\s*\*|\w+(?:<[^>]*>)?\s*\*?)\s*(\w+)(\[(\w+)\])?(\s*:\s*\d+)?$')
def parse_header(path):
    h=open(path,encoding='latin-1').read().split('\n')
    classes={}; order=[]; i=0
    while i<len(h):
        m=re.match(r'\s*class\s+(?:ENGINE_API\s+|CORE_API\s+)?(\w+)\s*:\s*public\s+(\w+)\s*$',h[i])
        if m:
            name,sup=m.group(1),m.group(2); mem=[]; j=i+1
            while j<len(h):
                t=h[j].strip()
                if 'DECLARE_CLASS' in t or t.startswith('};'): break
                if not t or t in('{','public:') or t.startswith('//') or t.startswith('#'): j+=1; continue
                raw=t; pk='GCC_PACK' in raw; al='GCC_ALIGN' in raw
                t2=re.sub(r'GCC_PACK\(\d\)|GCC_ALIGN\(\d\)','',t).strip()
                if t2.endswith(';') and '(' not in t2:
                    mm=MEM.match(t2[:-1].strip())
                    if mm:
                        ty=re.sub(r'\s+','',mm.group(1)) if '*' in mm.group(1) else mm.group(1).strip()
                        mem.append(dict(line=j+1,type=ty,name=mm.group(2),dim=mm.group(4),bits=mm.group(5),pack=pk,align=al,raw=raw))
                        j+=1; continue
                break
            while j<len(h) and 'DECLARE_CLASS' not in h[j]: j+=1
            classes[name]=dict(sup=sup,mem=mem,line=i+1)
            order.append(name); i=j
        i+=1
    return classes,order
