#!/usr/bin/env python3
# ucdis.py -- UnrealScript bytecode disassembler for HP2 (UE1 build 433) packages, for packages shipped without
# script text (the retail HGame/UWindow/...).   python3 tools/dev/retail/ucdis.py <pkg.u> <Class>[.<Function|State>] [<natives.txt>]
# Grammar = Core/Src/UnClass.cpp UStruct::SerializeExpr (HP2 variant: optional EX_DebugInfo after calls, BoolVariable
# as a prefix). In the file, object refs and names are compact indexes; jump targets are in-memory offsets (4 bytes per
# ref/name), so the disassembler tracks both and prints memory offsets (the ones jumps use).
# natives.txt: "<index> <Class> <execName>" lines (grep of IMPLEMENT_FUNCTION/AUTOGENERATE_FUNCTION in Source/).
import os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'layout'))
from upkg import Pkg
EX = {0x00:'LocalVariable',0x01:'InstanceVariable',0x02:'DefaultVariable',0x04:'Return',0x05:'Switch',0x06:'Jump',
      0x07:'JumpIfNot',0x08:'Stop',0x09:'Assert',0x0A:'Case',0x0B:'Nothing',0x0C:'LabelTable',0x0D:'GotoLabel',
      0x0E:'EatString',0x0F:'Let',0x10:'DynArrayElement',0x11:'New',0x12:'ClassContext',0x13:'MetaCast',0x14:'LetBool',
      0x15:'LineNumber',0x16:'EndFunctionParms',0x17:'Self',0x18:'Skip',0x19:'Context',0x1A:'ArrayElement',
      0x1B:'VirtualFunction',0x1C:'FinalFunction',0x1D:'IntConst',0x1E:'FloatConst',0x1F:'StringConst',
      0x20:'ObjectConst',0x21:'NameConst',0x22:'RotationConst',0x23:'VectorConst',0x24:'ByteConst',0x25:'IntZero',
      0x26:'IntOne',0x27:'True',0x28:'False',0x29:'NativeParm',0x2A:'NoObject',0x2C:'IntConstByte',0x2D:'BoolVariable',
      0x2E:'DynamicCast',0x2F:'Iterator',0x30:'IteratorPop',0x31:'IteratorNext',0x32:'StructCmpEq',0x33:'StructCmpNe',
      0x34:'UnicodeStringConst',0x36:'StructMember',0x37:'DynArrayCount',0x38:'DebugInfo',0x39:'GlobalFunction'}
CONV = {0x3a:'RotatorToVector',0x3b:'ByteToInt',0x3c:'ByteToBool',0x3d:'ByteToFloat',0x3e:'IntToByte',0x3f:'IntToBool',
        0x40:'IntToFloat',0x41:'BoolToByte',0x42:'BoolToInt',0x43:'BoolToFloat',0x44:'FloatToByte',0x45:'FloatToInt',
        0x46:'FloatToBool',0x47:'ObjectToBool',0x48:'NameToBool',0x49:'StringToByte',0x4a:'StringToInt',0x4b:'StringToBool',
        0x4c:'StringToFloat',0x4d:'StringToVector',0x4e:'StringToRotator',0x4f:'VectorToBool',0x50:'VectorToRotator',
        0x51:'RotatorToBool',0x52:'ByteToString',0x53:'IntToString',0x54:'BoolToString',0x55:'FloatToString',
        0x56:'ObjectToString',0x57:'NameToString',0x58:'VectorToString',0x59:'RotatorToString',0x5a:'StringToName'}

class Dis:
    def __init__(s, pk, natives):
        s.pk, s.d, s.nat = pk, pk.d, natives
    def ref(s, r):
        if r == 0: return 'None'
        if r > 0:
            e = s.pk.exports[r-1]; o = e['outer']
            return (s.pk.exports[o-1]['name'] + '.' if o > 0 else '') + e['name']
        return s.pk.imports[-r-1]['name']
    def ci(s):
        v, s.p = s.pk.ci(s.p); return v
    def u8(s):
        v = s.d[s.p]; s.p += 1; s.m += 1; return v
    def u16(s):
        v = struct.unpack_from('<H', s.d, s.p)[0]; s.p += 2; s.m += 2; return v
    def i32(s):
        v = struct.unpack_from('<i', s.d, s.p)[0]; s.p += 4; s.m += 4; return v
    def f32(s):
        v = struct.unpack_from('<f', s.d, s.p)[0]; s.p += 4; s.m += 4; return v
    def obj(s):
        r = s.ci(); s.m += 4; return s.ref(r)
    def name(s):
        n = s.ci(); s.m += 4; return s.pk.names[n]
    def cstr(s):
        b = bytearray()
        while True:
            c = s.u8()
            if c == 0: break
            b.append(c)
        return b.decode('latin-1')
    def debuginfo(s):  # HANDLE_OPTIONAL_DEBUG_INFO: a trailing EX_DebugInfo with version 100
        if s.d[s.p] == 0x38 and struct.unpack_from('<i', s.d, s.p+1)[0] == 100:
            s.expr()
    def call_parms(s):
        args = []
        while True:
            if s.d[s.p] == 0x16:
                s.u8(); break
            args.append(s.expr())
        s.debuginfo()
        return args
    def expr(s):
        t = s.u8()
        if 0x3a <= t < 0x60:
            return f'{CONV.get(t, "Conv%02x" % t)}({s.expr()})'
        if t >= 0x70 or 0x60 <= t < 0x70:
            idx = t if t >= 0x70 else ((t - 0x60) << 8) + s.u8()
            fn = s.nat.get(idx, f'native{idx}')
            return f'{fn}({", ".join(s.call_parms())})'
        n = EX.get(t, f'?{t:02x}')
        if n in ('Let', 'LetBool'): return f'{s.expr()} = {s.expr()}'
        if n == 'Jump': return f'goto {s.u16():04x}'
        if n == 'JumpIfNot':
            off = s.u16(); return f'if !({s.expr()}) goto {off:04x}'
        if n in ('LocalVariable', 'InstanceVariable', 'DefaultVariable', 'NativeParm'):
            v = s.obj().split('.')[-1]; return {'LocalVariable': v, 'InstanceVariable': v, 'DefaultVariable': 'Default.' + v}.get(n, v)
        if n == 'BoolVariable': return s.expr()
        if n == 'DebugInfo':
            ver, line, pos = s.i32(), s.i32(), s.i32(); s.cstr(); return f'/*dbg v{ver} line {line}*/'
        if n in ('Nothing', 'EndFunctionParms', 'IteratorPop', 'Stop', 'IteratorNext'): return n.lower()
        if n == 'IntZero': return '0'
        if n == 'IntOne': return '1'
        if n == 'True': return 'true'
        if n == 'False': return 'false'
        if n == 'NoObject': return 'None'
        if n == 'Self': return 'self'
        if n in ('EatString', 'Return'):
            return f'{n.lower()} {s.expr()}'.rstrip()
        if n == 'FinalFunction':
            f = s.obj(); return f'{f.split(".")[-1]}({", ".join(s.call_parms())})'
        if n in ('VirtualFunction', 'GlobalFunction'):
            f = s.name(); return f'{"global." if n == "GlobalFunction" else ""}{f}({", ".join(s.call_parms())})'
        if n in ('Context', 'ClassContext'):
            o = s.expr(); s.u16(); s.u8(); c = s.expr()
            return f'{o}{".static" if n == "ClassContext" else ""}.{c}'
        if n in ('ArrayElement', 'DynArrayElement'):
            i = s.expr(); b = s.expr(); return f'{b}[{i}]'
        if n == 'New':
            a = [s.expr() for _ in range(4)]; return f'new({a[0]},{a[1]},{a[2]}) {a[3]}'
        if n == 'IntConst': return str(s.i32())
        if n == 'FloatConst': return repr(round(s.f32(), 6))
        if n == 'StringConst': return '"' + s.cstr() + '"'
        if n == 'UnicodeStringConst':
            b = []
            while True:
                c = s.u16()
                if c == 0: break
                b.append(chr(c))
            return 'u"' + ''.join(b) + '"'
        if n == 'ObjectConst': return s.obj()
        if n == 'NameConst': return "'" + s.name() + "'"
        if n == 'RotationConst': return f'rot({s.i32()},{s.i32()},{s.i32()})'
        if n == 'VectorConst': return f'vect({s.f32():g},{s.f32():g},{s.f32():g})'
        if n in ('ByteConst', 'IntConstByte'): return str(s.u8())
        if n in ('MetaCast', 'DynamicCast'):
            c = s.obj(); return f'{c}({s.expr()})'
        if n == 'Iterator':
            e = s.expr(); off = s.u16(); return f'foreach {e} /*end {off:04x}*/'
        if n == 'Switch':
            s.u8(); return f'switch ({s.expr()})'
        if n == 'Assert':
            s.u16(); return f'assert({s.expr()})'
        if n == 'Case':
            off = s.u16()
            return 'default:' if off == 0xFFFF else f'case {s.expr()}: /*next {off:04x}*/'
        if n == 'LabelTable':
            out = []
            while True:
                nm = s.name(); ic = s.i32()
                if nm == 'None': break
                out.append(f'{nm}@{ic:04x}')
            return 'labels ' + ' '.join(out)
        if n == 'GotoLabel': return f'goto label {s.expr()}'
        if n == 'Skip':
            s.u16(); return s.expr()
        if n in ('StructCmpEq', 'StructCmpNe'):
            s.obj(); a = s.expr(); b = s.expr(); return f'({a} {"==" if n == "StructCmpEq" else "!="} {b})'
        if n == 'StructMember':
            p = s.obj().split('.')[-1]; return f'{s.expr()}.{p}'
        if n == 'DynArrayCount': return f'{s.expr()}.Length'
        raise ValueError(f'bad token {t:02x} at file {s.p-1:x}')
    def function(s, e):
        s.p = e['off']
        if e['flags'] & 0x02000000:  # RF_HasStack
            node = s.ci(); s.ci(); s.p += 12
            if node: s.ci()
        while True:  # tagged properties until None
            t = s.ci(); nm = s.pk.names[t]
            if nm == 'None': break
            raise ValueError('tagged props on a function not handled')
        s.ci(); s.ci()          # UField: Super, Next
        s.ci(); s.ci(); s.ci()  # UStruct: ScriptText, Children, FriendlyName
        s.p += 8                # Line, TextPos
        size = struct.unpack_from('<i', s.d, s.p)[0]; s.p += 4
        s.m = 0; lines = []
        while s.m < size:
            at = s.m
            try:
                lines.append(f'{at:04x}: {s.expr()}')
            except Exception as ex:
                lines.append(f'{at:04x}: !! {ex}'); break
        return lines

def main():
    pk = Pkg(sys.argv[1]); target = sys.argv[2]
    nat = {}
    nf = sys.argv[3] if len(sys.argv) > 3 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'natives.txt')
    if os.path.exists(nf):
        for line in open(nf):
            i, c, f = line.split()
            nat[int(i)] = f.replace('exec', '', 1)
    cls, _, fn = target.partition('.')
    ce = [e for e in pk.exports if e['name'].lower() == cls.lower() and pk.clsname(e) == 'Class']
    if not ce: sys.exit(f'no class {cls}')
    ci = ce[0]['idx'] + 1
    fns = [e for e in pk.exports if e['outer'] == ci and pk.clsname(e) in ('Function', 'State')
           and (not fn or e['name'].lower() == fn.lower())]
    d = Dis(pk, nat)
    for e in fns:
        print(f'== {cls}.{e["name"]} ({pk.clsname(e)})')
        for l in d.function(e): print('   ' + l)
main()
