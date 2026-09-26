#!/usr/bin/env python3
"""unifdef-false.py MACRO file...  -- resolve preprocessor groups whose condition is exactly MACRO as FALSE.

Handles `#if MACRO`, `#ifdef MACRO`, `#if defined(MACRO)`, `#if !MACRO`, `#ifndef MACRO`, `#if !defined(MACRO)`
and the same forms after `#elif`. Other groups are left untouched (nesting-aware). Used to drop the PS2
(`__PSX2_EE__`) and similar console branches that the port never builds (PLAN §1 rule 2). No unifdef on this box.
"""
import re, sys

def cond_kind(expr, m):
    e = re.sub(r'\s+', '', expr.split('//')[0].split('/*')[0])
    if e in (m, 'defined(%s)' % m, 'defined%s' % m): return 'F'
    if e in ('!' + m, '!defined(%s)' % m): return 'T'
    return None

def process(lines, m):
    out = []; stack = []  # each: dict(mode) mode: 'keep' (untouched group), 'F' (skipping), 'T' (keep, drop directive)
    def emitting():
        return all(s['emit'] for s in stack)
    for ln in lines:
        d = re.match(r'\s*#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)', ln)
        if not d:
            if emitting(): out.append(ln)
            continue
        kw, rest = d.group(1), d.group(2)
        if kw in ('if', 'ifdef', 'ifndef'):
            k = None
            if kw == 'ifdef': k = 'F' if rest.strip() == m else None
            elif kw == 'ifndef': k = 'T' if rest.strip() == m else None
            else: k = cond_kind(rest, m)
            if k is None:
                stack.append({'kind': 'plain', 'emit': True, 'done': False})
                if emitting(): out.append(ln)
            else:
                # resolved group: emit its branch contents only when TRUE
                stack.append({'kind': 'resolved', 'emit': k == 'T', 'done': k == 'T', 'opened': False})
            continue
        top = stack[-1]
        if kw == 'elif':
            k = cond_kind(rest, m)
            if top['kind'] == 'plain':
                if k == 'F':
                    # branch never taken: skip until next elif/else/endif
                    top['skip_branch'] = True; top['emit'] = False
                    continue
                top['emit'] = True; top.pop('skip_branch', None)
                if all(s['emit'] for s in stack[:-1]): out.append(ln)
                continue
            # resolved group
            if top['done']:
                top['emit'] = False; continue
            if k == 'F': top['emit'] = False; continue
            if k == 'T': top['emit'] = True; top['done'] = True; continue
            # a real condition becomes the group's new #if
            top['kind'] = 'plain'; top['emit'] = True
            if all(s['emit'] for s in stack[:-1]): out.append(re.sub(r'#\s*elif', '#if', ln, count=1))
            continue
        if kw == 'else':
            if top['kind'] == 'plain':
                top['emit'] = True
                if all(s['emit'] for s in stack[:-1]): out.append(ln)
                continue
            top['emit'] = not top['done']; top['done'] = True
            continue
        if kw == 'endif':
            s = stack.pop()
            if s['kind'] == 'plain' and emitting(): out.append(ln)
            continue
    return out

if __name__ == '__main__':
    m = sys.argv[1]
    for f in sys.argv[2:]:
        src = open(f, newline='').read().split('\n')
        new = process(src, m)
        if new != src:
            open(f, 'w', newline='').write('\n'.join(new)); print('rewrote', f)
