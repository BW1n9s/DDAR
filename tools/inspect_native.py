"""Read-only PE and Address Library inspection; generated reports stay local.
Requires Capstone, installed separately. Does not load or patch any image.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def read(self, fmt):
        result = struct.unpack_from('<' + fmt, self.data, self.pos)
        self.pos += struct.calcsize('<' + fmt)
        return result[0] if len(result) == 1 else result


def addresses(path, wanted):
    r = Reader(path.read_bytes())
    fmt = r.read('i')
    if fmt != 1:
        raise ValueError('Only local SE v1 Address Library is supported')
    version = r.read('4i')
    name_len = r.read('i')
    if not 0 <= name_len < 4096:
        raise ValueError('Invalid executable name length')
    r.pos += name_len
    pointer_size, count = r.read('2i')
    if pointer_size != 8 or not 0 < count < 10000000:
        raise ValueError('Invalid Address Library header')
    result, prev_id, prev_offset = {}, 0, 0

    def decode(kind, previous):
        if kind == 0: return r.read('Q')
        if kind == 1: return previous + 1
        if kind == 2: return previous + r.read('B')
        if kind == 3: return previous - r.read('B')
        if kind == 4: return previous + r.read('H')
        if kind == 5: return previous - r.read('H')
        if kind == 6: return r.read('H')
        if kind == 7: return r.read('I')
        raise ValueError('Unsupported address encoding')

    for _ in range(count):
        kind = r.read('B')
        ident = decode(kind & 15, prev_id)
        scale = bool(kind & 128)
        offset = decode((kind >> 4) & 7, prev_offset // 8 if scale else prev_offset)
        if scale: offset *= 8
        if ident in wanted: result[ident] = offset
        prev_id, prev_offset = ident, offset
    return {'version': version, 'offsets': result}


class PE:
    def __init__(self, path):
        self.data = path.read_bytes()
        self.sha256 = hashlib.sha256(self.data).hexdigest()
        nt = struct.unpack_from('<I', self.data, 0x3C)[0]
        if self.data[nt:nt + 4] != b'PE\0\0': raise ValueError('Not PE')
        machine, count = struct.unpack_from('<HH', self.data, nt + 4)
        size = struct.unpack_from('<H', self.data, nt + 20)[0]
        if machine != 0x8664: raise ValueError('Expected AMD64')
        self.base = struct.unpack_from('<Q', self.data, nt + 24 + 24)[0]
        self.sections = []
        for i in range(count):
            o = nt + 24 + size + i * 40
            vs, va, raw_size, raw = struct.unpack_from('<4I', self.data, o + 8)
            self.sections.append((va, vs, raw, raw_size))

    def read(self, rva, size):
        for va, vs, raw, raw_size in self.sections:
            if va <= rva and rva + size <= va + raw_size:
                return self.data[raw + rva - va:raw + rva - va + size]
        raise ValueError(f'RVA not backed by file data: {rva:X}')


def disassemble(pe, rva, size=256):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    code = pe.read(rva, size)
    return {'rva': f'{rva:X}', 'prefix32': code[:32].hex(),
            'instructions': [f'{i.address:X}: {i.mnemonic} {i.op_str}'
                             for i in Cs(CS_ARCH_X86, CS_MODE_64).disasm(code, pe.base + rva)]}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--exe', type=Path, required=True)
    ap.add_argument('--ddng', type=Path, required=True)
    ap.add_argument('--address-library', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    offsets = addresses(args.address_library, {17359, 17361, 19316, 24232})
    exe, dd = PE(args.exe), PE(args.ddng)
    result = {'exeSHA256': exe.sha256, 'ddngSHA256': dd.sha256,
              'addressLibrary': offsets, 'engine': {},
              'ddngGetAPI': disassemble(dd, 0x5F40, 128)}
    for ident, offset in offsets['offsets'].items():
        result['engine'][ident] = disassemble(exe, offset)
    # Known local export RVA comes from dumpbin /exports, not an assumed upstream address.
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__': main()
