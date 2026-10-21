"""Read built ESP/SEQ/PEX/DLL artifacts and check their cross-language wiring.
This does not launch Skyrim or prove runtime appearance/locking behavior.
"""
import argparse
import re
import struct
from pathlib import Path

from audit_installation import subrecords, vmad_scripts


def require(condition, message):
    if not condition:
        raise ValueError(message)


def pex(path):
    data = path.read_bytes()
    require(data[:8] == bytes.fromhex('FA57C0DE03020001'), f'Not Skyrim PEX 3.2: {path}')
    pos = 16

    def read(fmt):
        nonlocal pos
        value = struct.unpack_from('>' + fmt, data, pos)[0]
        pos += struct.calcsize('>' + fmt)
        return value

    def string():
        nonlocal pos
        size = read('H')
        value = data[pos:pos + size].decode('utf-8')
        pos += size
        return value

    for _ in range(3):
        string()  # source path, username, computer
    strings = [string() for _ in range(read('H'))]
    if read('B'):
        read('Q')
        for _ in range(read('H')):
            read('H'); read('H'); read('H'); read('B')
            for _ in range(read('H')):
                read('H')
    for _ in range(read('H')):
        read('H'); read('B')
    require(read('H') == 1, f'Expected one object: {path}')
    name = strings[read('H')]
    read('I')
    parent = strings[read('H')]
    return name, parent, set(strings)


def plugin_records(data, begin=0, end=None):
    end = len(data) if end is None else end
    pos = begin
    while pos < end:
        require(pos + 24 <= end, 'Truncated ESP header')
        tag, size = struct.unpack_from('<4sI', data, pos)
        if tag == b'GRUP':
            require(size >= 24 and pos + size <= end, 'Invalid ESP group')
            yield from plugin_records(data, pos + 24, pos + size)
            pos += size
        else:
            stop = pos + 24 + size
            require(stop <= end, 'Truncated ESP record')
            yield tag, data[pos:pos + 24], data[pos + 24:stop]
            pos = stop
    require(pos == end, 'Record boundary mismatch')


def verify(stage, repo):
    native = (repo / 'papyrus/DDAR_Native.psc').read_text(encoding='utf-8')
    bridge = (repo / 'src/McmBridge.cpp').read_text(encoding='utf-8')
    declared = set(re.findall(r'Function\s+(\w+)\(', native, re.I))
    registered = set(re.findall(r'RegisterFunction\("(\w+)","DDAR_Native"', bridge))
    require(declared == registered, f'Papyrus/native mismatch: {declared ^ registered}')
    dll = (stage / 'SKSE/Plugins/DDARProbe.dll').read_bytes()
    require(dll[:2] == b'MZ', 'Missing native PE DLL')
    pe_offset = struct.unpack_from('<I', dll, 60)[0]
    require(dll[pe_offset:pe_offset+6] == b'PE\0\0\x64\x86', 'Expected x64 DLL')
    for entry in declared | {'DDAR_Native', 'SKSEPlugin_Load', 'SKSEPlugin_Query'}:
        require(entry.encode() + b'\0' in dll, f'Missing native binding/export: {entry}')
    expected = {'DDAR_Native': '', 'DDAR_MCM': 'SKI_ConfigBase', 'DDAR_PlayerAlias': 'ReferenceAlias'}
    for name, parent in expected.items():
        actual, base, strings = pex(stage / f'Scripts/{name}.pex')
        require((actual.lower(), base.lower()) == (name.lower(), parent.lower()), f'PEX inheritance mismatch: {name}')
        if name == 'DDAR_Native':
            require(declared <= strings, 'Native PEX functions missing')
        elif name == 'DDAR_MCM':
            require({'OnConfigInit', 'OnPageReset', 'OnOptionSelect', 'OnOptionMenuOpen', 'OnOptionMenuAccept'} <= strings, 'MCM callbacks missing')
        else:
            require({'OnPlayerLoadGame', 'OnGameReload'} <= strings, 'Reload callback missing')
    records = list(plugin_records((stage / 'DDAR.esp').read_bytes()))
    require([r[0] for r in records] == [b'TES4', b'QUST'], 'ESP must contain only header and own quest')
    header, quest = records
    require(struct.unpack_from('<I', header[1], 8)[0] & 0x200, 'ESP is not ESL flagged')
    masters = [value.rstrip(b'\0').decode() for tag, value in subrecords(header[2]) if tag == 'MAST']
    require(masters == ['Skyrim.esm'], f'Unexpected masters: {masters}')
    fid = struct.unpack_from('<I', quest[1], 12)[0]
    require(fid >> 24 == len(masters) and (fid & 0xffffff) == 0x800, 'Quest is not an own file-local record')
    require(struct.unpack_from('<H', quest[1], 20)[0] == 44, 'Quest is not SE 1.5.97 format 44')
    seq = (stage / 'SEQ/DDAR.seq').read_bytes()
    require(seq == struct.pack('<I', fid), 'SEQ does not match serialized quest FixedFormID')
    fields = dict(subrecords(quest[2]))
    require(struct.unpack_from('<H', fields['DNAM'])[0] & 1, 'Quest not Start Game Enabled')
    require(vmad_scripts(fields['VMAD'])[0]['name'] == 'DDAR_MCM', 'Quest MCM binding missing')
    require(b'DDAR_PlayerAlias' in fields['VMAD'], 'Player reload alias binding missing')
    print(f'Artifact checks passed: x64 native DLL, {len(declared)} native bindings, 3 Skyrim PEX inheritance/callback sets, ESL quest, VMAD, FixedFormID SEQ.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, default=Path('F:/DDAR-Dev/Workspace/stage/DDARProbe'))
    options = parser.parse_args()
    verify(options.stage, Path(__file__).resolve().parents[1])
