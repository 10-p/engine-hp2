#!/usr/bin/env python3
"""msvcread.py FILE LINE VAR -- the for at LINE declares VAR. Report whether VAR is READ after the for
statement, inside the same enclosing block, before being written/redeclared (i.e. code that relied on MSVC6
for-scope and now silently binds to an outer VAR)."""
import re, sys

def tokens(src):
    # strip comments and string/char literals, keep line numbers
    out = []
    i, n, line = 0, len(src), 1
    tokre = re.compile(r'[A-Za-z_]\w*|\d[\w.]*|->|::|==|!=|<=|>=|\+\+|--|&&|\|\||[-+*/%&|^]=|<<|>>|\S')
    while i < n:
        c = src[i]
        if c == '\n': line += 1; i += 1; continue
        if c.isspace(): i += 1; continue
        if src.startswith('//', i):
            j = src.find('\n', i); i = n if j < 0 else j; continue
        if src.startswith('/*', i):
            j = src.find('*/', i + 2); j = n if j < 0 else j + 2
            line += src.count('\n', i, j); i = j; continue
        if c == '#' and (i == 0 or src.rfind('\n', 0, i) == i - 1 or src[src.rfind('\n', 0, i)+1:i].strip() == ''):
            # preprocessor line (with continuations)
            j = i
            while True:
                k = src.find('\n', j)
                if k < 0: k = n; break
                if src[k-1] == '\\': j = k + 1; line += 1; continue
                break
            i = k; continue
        if c in '"\'':
            j = i + 1
            while j < n and src[j] != c:
                if src[j] == '\\': j += 1
                j += 1
            out.append(('LIT', line)); i = j + 1; continue
        m = tokre.match(src, i)
        out.append((m.group(0), line)); i = m.end()
    return out

def match(toks, i, o, c):
    d = 0
    while i < len(toks):
        if toks[i][0] == o: d += 1
        elif toks[i][0] == c:
            d -= 1
            if d == 0: return i
        i += 1
    raise ValueError

def stmt_end(toks, i):
    t = toks[i][0]
    if t == '{': return match(toks, i, '{', '}')
    if t in ('for', 'while', 'switch'):
        j = match(toks, i + 1, '(', ')'); return stmt_end(toks, j + 1)
    if t == 'if':
        j = match(toks, i + 1, '(', ')'); e = stmt_end(toks, j + 1)
        if e + 1 < len(toks) and toks[e + 1][0] == 'else': return stmt_end(toks, e + 2)
        return e
    if t == 'do':
        e = stmt_end(toks, i + 1)  # body
        j = match(toks, e + 2, '(', ')'); return j + 1
    d = 0
    while True:
        tk = toks[i][0]
        if tk in '([{': d += 1
        elif tk in ')]}': d -= 1
        elif tk == ';' and d == 0: return i
        i += 1

def check(path, line, var):
    toks = tokens(open(path, errors='replace').read())
    idx = [k for k, (t, l) in enumerate(toks) if t == 'for' and l == line]
    res = []
    for k in idx:
        hdr_end = match(toks, k + 1, '(', ')')
        if var not in [t for t, _ in toks[k:hdr_end]]: continue
        end = stmt_end(toks, hdr_end + 1)
        # enclosing block end
        d, j = 0, end + 1
        while j < len(toks):
            if toks[j][0] == '{': d += 1
            elif toks[j][0] == '}':
                if d == 0: break
                d -= 1
            if toks[j][0] == var and toks[j-1][0] not in ('.', '->', '::'):
                prev, nxt = toks[j-1][0], toks[j+1][0]
                if nxt == '=' or re.match(r'[A-Za-z_]\w*$', prev) and prev not in ('return', 'case', 'else', 'do'):
                    res.append('ok(write/decl)@%d' % toks[j][1]); break
                if prev == '*' and re.match(r'[A-Za-z_]', toks[j-2][0]) and toks[j-3][0] in (';', '{', '}', '('):
                    res.append('ok(ptr decl)@%d' % toks[j][1]); break
                res.append('READ@%d' % toks[j][1]); break
            j += 1
        else:
            res.append('none')
        if not res or res[-1] == 'none' or j >= len(toks) or toks[j][0] == '}':
            if not res: res.append('none(block end %d)' % toks[j][1])
    return res

if __name__ == '__main__':
    for arg in sys.argv[1:] if len(sys.argv) > 2 and ':' in sys.argv[1] else [None]:
        pass
    path, line, var = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    print(path.split('/')[-1], line, var, check(path, line, var))
