#!/usr/bin/env python3
# msvcscan.py FILE... -- run msvcread.check on every for-init declaration (and for_array/for_count) in each file
# and print those whose variable is read after the loop.
import sys, re
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from msvcread import tokens, match, check
ID = re.compile(r'[A-Za-z_]\w*$')
for path in sys.argv[1:]:
    toks = tokens(open(path, errors='replace').read())
    seen = set()
    for k, (t, l) in enumerate(toks):
        names = []
        if t in ('for_array', 'for_count') and toks[k+1][0] == '(':
            names = [toks[k+2][0]]
        elif t == 'for' and toks[k+1][0] == '(':
            e = match(toks, k+1, '(', ')')
            init = []
            j = k + 2
            while j < e and toks[j][0] != ';': init.append(toks[j][0]); j += 1
            if len(init) >= 2 and ID.match(init[0]) and (ID.match(init[1]) or init[1] in ('*', '&', '<', '::')):
                d = 0
                for a in range(1, len(init)):
                    x = init[a]
                    if x in '(<[': d += 1 if x != '<' else 0
                    if x in ')]': d -= 1
                    if ID.match(x) and a + 1 < len(init) and init[a+1] in ('=', '(') and (ID.match(init[a-1]) or init[a-1] in ('*', '&', '>', ',')):
                        names.append(x)
                    elif ID.match(x) and a + 1 == len(init) and (ID.match(init[a-1]) or init[a-1] in ('*', '>')):
                        names.append(x)
        for v in names:
            if (l, v) in seen: continue
            seen.add((l, v))
            try:
                r = check(path, l, v)
            except Exception as ex:
                r = ['ERR %s' % ex]
            if any(x.startswith('READ') for x in r):
                print('%s:%d %s %s' % (path.split('/')[-1], l, v, r))
