"""Read-only MO2/Skyrim record audit. Python 3.12+, standard library only.

This is NOT an xEdit validator, NIF validator, or an in-game test.
MO2 modlist.txt is stored in descending priority. Archives are not extracted.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zlib
from functools import cache
from datetime import datetime, timezone
from pathlib import Path


class FormatError(ValueError):
    pass


def u32(data, offset=0):
    return struct.unpack_from('<I', data, offset)[0]


def ztext(data, encoding='utf-8'):
    return data.rstrip(b'\0').decode(encoding, errors='replace')


def subrecords(data):
    pos, extended = 0, None
    while pos < len(data):
        if pos + 6 > len(data):
            raise FormatError('Truncated subrecord header')
        tag, size = struct.unpack_from('<4sH', data, pos)
        pos += 6
        if tag == b'XXXX':
            if size != 4 or pos + 4 > len(data):
                raise FormatError('Invalid XXXX')
            extended = u32(data, pos)
            pos += 4
            continue
        size = extended if extended is not None else size
        extended = None
        if pos + size > len(data):
            raise FormatError(f'Truncated {tag!r}')
        yield tag.decode('ascii'), data[pos:pos + size]
        pos += size
    if extended is not None:
        raise FormatError('Dangling XXXX')


class Cursor:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, size):
        if size < 0 or self.pos + size > len(self.data):
            raise FormatError('Truncated VMAD')
        result = self.data[self.pos:self.pos + size]
        self.pos += size
        return result

    def number(self, fmt):
        return struct.unpack(fmt, self.take(struct.calcsize(fmt)))[0]

    def string(self):
        return self.take(self.number('<H')).decode('utf-8', errors='replace')


def vmad_scripts(data):
    c = Cursor(data)
    version, objformat, count = c.number('<H'), c.number('<H'), c.number('<H')
    if version > 5 or objformat not in (1, 2):
        raise FormatError(f'Unsupported VMAD {version}/{objformat}')

    def value(kind):
        if kind == 1:
            raw = c.take(8)
            return {'form': u32(raw, 0 if objformat == 1 else 4)}
        if kind == 2:
            return c.string()
        if kind == 3:
            return c.number('<i')
        if kind == 4:
            return c.number('<f')
        if kind == 5:
            return bool(c.number('<B'))
        if kind in (11, 12, 13, 14, 15):
            n = c.number('<I')
            if n > len(data):
                raise FormatError('Invalid VMAD array count')
            return [value(kind - 10) for _ in range(n)]
        raise FormatError(f'Unsupported VMAD property type {kind}')

    result = []
    for _ in range(count):
        name, status, props = c.string(), c.number('<B'), {}
        for _ in range(c.number('<H')):
            key, kind, propstatus = c.string(), c.number('<B'), c.number('<B')
            props[key] = value(kind)
        result.append({'name': name, 'status': status, 'properties': props})
    return result


def canonical(plugin, masters, fid):
    if fid == 0:
        return None
    index, local = fid >> 24, fid & 0xFFFFFF
    owners = masters + [plugin]
    if index >= len(owners):
        raise FormatError(f'Invalid file-local master index: {fid:08X} in {plugin}')
    # These are ON-DISK FormIDs, not FE runtime IDs. No light-index shifting here.
    return f'{owners[index].casefold()}|{local:06X}'


def records(path):
    """Skip unrelated top-level groups; never write the plugin."""
    size = path.stat().st_size
    with path.open('rb') as f:
        def walk(end):
            while f.tell() < end:
                start = f.tell()
                h = f.read(24)
                if len(h) != 24:
                    raise FormatError('Truncated record header')
                tag, length, flags, fid = struct.unpack_from('<4sIII', h)
                if tag == b'GRUP':
                    stop = start + length
                    if length < 24 or stop > end:
                        raise FormatError('Invalid group length')
                    group_type = struct.unpack_from('<i', h, 12)[0]
                    if group_type == 0 and h[8:12] not in (b'ARMO', b'ARMA', b'KYWD'):
                        f.seek(stop)
                    else:
                        yield from walk(stop)
                else:
                    if f.tell() + length > end:
                        raise FormatError('Invalid record length')
                    if tag not in (b'TES4', b'ARMO', b'ARMA', b'KYWD'):
                        f.seek(length, 1)
                        continue
                    data = f.read(length)
                    if flags & 0x40000:
                        expected = u32(data)
                        data = zlib.decompress(data[4:])
                        if len(data) != expected:
                            raise FormatError('Invalid decompressed size')
                    yield tag.decode('ascii'), flags, fid, data
        yield from walk(size)


def parse_plugin(path, text_encoding='utf-8'):
    masters, entries, header = [], [], {}
    for kind, flags, fid, raw in records(path):
        subs = list(subrecords(raw))
        if kind == 'TES4':
            masters = [ztext(v) for k, v in subs if k == 'MAST']
            header = {'light': bool(flags & 0x200), 'localized': bool(flags & 0x80)}
            continue
        key = canonical(path.name, masters, fid)
        obj = {'key': key, 'type': kind, 'winningPlugin': path.name,
               'deleted': bool(flags & 0x20), 'editorID': '', 'name': '',
               'slots': [], 'addons': [], 'models': {}, 'keywords': [], 'scripts': []}
        for k, v in subs:
            if k == 'EDID':
                obj['editorID'] = ztext(v)
            elif k == 'FULL':
                obj['name'] = f'<localized:{u32(v):08X}>' if header['localized'] else ztext(v, text_encoding)
            elif k in ('BOD2', 'BODT'):
                mask = u32(v)
                obj['slots'] = [i + 30 for i in range(32) if mask & (1 << i)]
            elif kind == 'ARMO' and k == 'MODL':
                if len(v) % 4:
                    raise FormatError('Invalid ARMO armature references')
                obj['addons'].extend(canonical(path.name, masters, u32(v, i)) for i in range(0, len(v), 4))
            elif kind == 'ARMA' and k in ('MOD2', 'MOD3', 'MOD4', 'MOD5'):
                obj['models'][k] = ztext(v)
            elif k == 'KWDA':
                obj['keywords'] = [canonical(path.name, masters, u32(v, i)) for i in range(0, len(v), 4)]
            elif k == 'VMAD':
                try:
                    obj['scripts'] = vmad_scripts(v)
                    for script in obj['scripts']:
                        for prop, val in script['properties'].items():
                            if isinstance(val, dict) and 'form' in val:
                                script['properties'][prop] = {'key': canonical(path.name, masters, val['form'])}
                except (FormatError, struct.error) as exc:
                    obj['vmadError'] = str(exc)
        entries.append(obj)
    return header, entries


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def audit(mo2, game, profile, text_encoding='utf-8'):
    p = mo2 / 'profiles' / profile
    lines = (p / 'modlist.txt').read_text('utf-8-sig').splitlines()
    enabled = [line[1:] for line in lines if line.startswith('+')]
    roots = [mo2 / 'overwrite'] + [mo2 / 'mods' / name for name in enabled] + [game / 'Data']
    # MO2 unmanaged/plugin-specific asset rules and archives require live VFS verification.
    @cache
    def resolve(relative):
        rel = Path(relative.replace('\\', '/'))
        if rel.is_absolute() or '..' in rel.parts or rel.drive:
            return None
        return next((r / rel for r in roots if (r / rel).is_file()), None)

    plugin_lines = (p / 'plugins.txt').read_text('utf-8-sig').splitlines()
    active = [s[1:] for s in plugin_lines if s.startswith('*')]
    ordered = ['Skyrim.esm', 'Update.esm', 'Dawnguard.esm', 'HearthFires.esm', 'Dragonborn.esm']
    ordered += [n for n in active if n.casefold() not in {x.casefold() for x in ordered}]
    database, plugins, errors = {}, [], []
    for name in ordered:
        path = resolve(name)
        if path is None:
            errors.append(f'Missing enabled plugin: {name}')
            continue
        try:
            header, objs = parse_plugin(path, text_encoding)
            plugins.append({'name': name, 'path': str(path), 'sha256': digest(path), **header})
            database.update((o['key'], o) for o in objs)
        except (OSError, ValueError, struct.error, zlib.error) as exc:
            errors.append(f'{name}: {exc}')

    def describe(obj):
        result = dict(obj)
        result['keywordEditorIDs'] = [database.get(k, {}).get('editorID', k) for k in obj['keywords']]
        result['armatures'] = []
        for key in obj['addons']:
            addon = database.get(key)
            if not addon or addon['deleted'] or addon['type'] != 'ARMA':
                result['armatures'].append({'key': key, 'error': 'ARMA not resolved'})
                continue
            a = {k: addon[k] for k in ('key', 'winningPlugin', 'editorID', 'slots', 'models')}
            a['looseModelFiles'] = {}
            for k, model in addon['models'].items():
                path = resolve('meshes/' + model) if model else None
                a['looseModelFiles'][k] = {'path': str(path) if path else None,
                    'status': 'loose-file-present-not-NIF-validated' if path else 'not-found-loose-may-be-in-BSA'}
            result['armatures'].append(a)
        return result

    source = [describe(o) for o in database.values() if o['type'] == 'ARMO' and not o['deleted']
              and 'magicwand belt' in o['key']]
    targets = []
    for obj in database.values():
        if obj['type'] != 'ARMO' or obj['deleted'] or 'devious devices' not in obj['key']:
            continue
        for script in obj['scripts']:
            props = {k.casefold(): v for k, v in script['properties'].items()}
            render_key = props.get('devicerendered', {}).get('key')
            inv_key = props.get('deviceinventory', {}).get('key')
            rendered = database.get(render_key)
            if not rendered or rendered['type'] != 'ARMO' or rendered['deleted']:
                continue
            kw = [database.get(k, {}).get('editorID', '') for k in rendered['keywords']]
            if 'zad_DeviousBelt' not in kw:
                continue
            targets.append({'inventory': describe(obj), 'rendered': describe(rendered),
                            'script': script['name'], 'inventoryPropertyMatches': inv_key == obj['key'],
                            'status': 'static-candidate-requires-DDNG-and-lock-tests'})
            break
    dlls = {}
    for relative in ('SKSE/Plugins/DeviousDevices.dll', 'SKSE/Plugins/DynamicArmorVariants.dll',
                     'SKSE/Plugins/version-1-5-97-0.bin'):
        path = resolve(relative)
        dlls[relative] = {'path': str(path) if path else None, 'sha256': digest(path) if path else None}
    return {'schemaVersion': 1, 'generatedUTC': datetime.now(timezone.utc).isoformat(),
            'profile': profile, 'mo2': str(mo2), 'game': str(game),
            'recordTextEncoding': text_encoding, 'incomplete': bool(errors),
            'limitations': ['Offline audit; no game execution, no lock/unlock test',
                'Loose file priority only; archives and live MO2 VFS not verified',
                'Localized names unresolved; no NIF partition, texture, skeleton or physics validation',
                'DD property links are candidates, not proof of runtime DDNG database membership'],
            'errors': errors, 'plugins': plugins, 'resolvedDependencies': dlls,
            'sourceCandidates': source, 'targetBeltCandidates': targets,
            'recordCount': len(database)}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--mo2', type=Path, required=True)
    ap.add_argument('--game', type=Path, required=True)
    ap.add_argument('--profile', required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--text-encoding', default='utf-8', help='Encoding of nonlocalized FULL text only')
    args = ap.parse_args()
    # Refuse to write under the supplied installations, including through symlinks.
    dest = args.output.resolve()
    for root in (args.mo2.resolve(), args.game.resolve()):
        if dest.is_relative_to(root):
            ap.error('Output must be outside the installation')
    result = audit(args.mo2, args.game, args.profile, args.text_encoding)
    dest.parent.mkdir(parents=True, exist_ok=True)
    temporary = dest.with_suffix(dest.suffix + '.tmp')
    temporary.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')
    temporary.replace(dest)
    print(json.dumps({'output': str(dest), 'plugins': len(result['plugins']),
        'records': result['recordCount'], 'sourceCandidates': len(result['sourceCandidates']),
        'targetBeltCandidates': len(result['targetBeltCandidates']), 'errors': result['errors']}, ensure_ascii=False))
    return 1 if result['errors'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
