# closure.py <Package>... -- the package dependency closure of the given roots in the content tree A (each file
# with its size, the total, packages with no file, and the import edges).
import sys, os, glob, json
sys.path.insert(0, os.path.dirname(__file__))
from upkg import Pkg
A = '/home/ion/hp2-ufrontend/HarryPotter2/Assets'
# Paths order from Default.ini
paths = [('System','.u'),('Maps','.unr'),('Textures','.utx'),('Sounds','.uax'),('Music','.umx'),('Save','.usa')]
index = {}
for d, ext in paths:
    for f in sorted(glob.glob(os.path.join(A, d, '*'))):
        if os.path.isfile(f) and f.lower().endswith(ext):
            n = os.path.basename(f)[:-len(ext)].lower()
            index.setdefault(n, os.path.relpath(f, A))
_cache = {}
def pkg(name):
    rel = index.get(name.lower())
    if not rel: return None
    if rel not in _cache: _cache[rel] = Pkg(os.path.join(A, rel))
    return _cache[rel]
def closure(roots):
    seen, missing, order = set(), set(), []
    stack = list(roots)
    edges = {}
    while stack:
        n = stack.pop(0)
        if n.lower() in seen: continue
        seen.add(n.lower())
        p = pkg(n)
        if not p: missing.add(n); continue
        order.append(index[n.lower()])
        tops = p.top_packages()
        edges[n] = tops
        for t in tops:
            if t.lower() not in seen: stack.append(t)
    return order, missing, edges
if __name__ == '__main__':
    roots = sys.argv[1:]
    order, missing, edges = closure(roots)
    tot = 0
    for r in order:
        s = os.path.getsize(os.path.join(A, r)); tot += s
        print('%-40s %12d' % (r, s))
    print('TOTAL', tot, '(%.1f MB)' % (tot/1048576), 'files', len(order))
    print('MISSING (no file):', sorted(missing))
    for k, v in edges.items(): print('  ', k, '->', ', '.join(v))
